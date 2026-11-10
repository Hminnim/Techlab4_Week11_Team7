#pragma once

#include "Object.h"
#include "Property.h"

class UStruct : public UObject
{
public:
	TArray<FProperty> Properties;

	const TArray<FProperty>& GetProperties() const { return Properties; }
	const FProperty* FindProperty(const FString& InName) const;

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset, uint32 InFlags = CPF_Default, const FPropertyMeta& InMeta = {})
	{
		FProperty Property = MakeProperty<T>(InName, InOffset, InFlags);
		Property.Meta = InMeta;
		Properties.Add(std::move(Property));
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset, EPropertyType InType,
		uint32 InFlags = CPF_Default, const FPropertyMeta& InMeta = {})
	{
		FProperty Property{ InName, InType, InOffset, sizeof(T), nullptr, InFlags };
		Property.Meta = InMeta;
		Property.bUnsignedInt = std::is_same_v<T, uint32>;
		Properties.Add(std::move(Property));
	}

protected:
	UStruct() : UObject(false) {}
	virtual const UStruct* GetSuperStruct() const { return nullptr; }

private:
	friend class UFunction;

	template <typename T>
	static FProperty MakeProperty(const FString& InName, uint64 InOffset, uint32 InFlags)
	{
		constexpr EPropertyType Type = GetPropertyType<T>();
		FProperty Property{ InName, Type, InOffset, sizeof(T), nullptr, InFlags };
		Property.bUnsignedInt = std::is_same_v<T, uint32>;
		if constexpr (std::is_pointer_v<T> &&
			std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
		{
			Property.Class = std::remove_pointer_t<T>::StaticClass();
		}
		else if constexpr (Type == EPropertyType::Struct)
		{
			// 구조체의 필드가 이 타입을 참조할 수 있으므로 조회할 때 등록한다.
			Property.ResolveScriptStruct = &T::StaticStruct;
		}
		else if constexpr (Type == EPropertyType::Enum)
		{
			static const FEnumPropertyOps EnumOps{
				TEnumInfo<T>::template Get<>(),
				[](const void* Value) { return static_cast<int64>(*static_cast<const T*>(Value)); },
				[](void* Value, int64 Number) { *static_cast<T*>(Value) = static_cast<T>(Number); },
			};
			Property.Enum = &EnumOps;
		}
		else if constexpr (Type == EPropertyType::Array)
		{
			using Element = typename T::ElementType;
			static_assert(!std::is_same_v<Element, bool>, "Reflected TArray elements must have an address. TArray<bool> uses packed bits.");
			static_assert(std::is_default_constructible_v<Element> && std::is_copy_constructible_v<Element>
				&& std::is_copy_assignable_v<Element>, "Reflected TArray elements must be default constructible and copyable.");
			static constexpr FArrayPropertyOps ArrayOps{
				[]() -> const FProperty&
				{
					// 원소가 소유 클래스의 포인터일 수 있으므로 실제 조회까지 등록을 미룬다.
					static const FProperty ElementProperty = MakeProperty<Element>("", 0, CPF_Default);
					return ElementProperty;
				},
				[](const void* Array) { return static_cast<const T*>(Array)->Num(); },
				[](void* Array, int32 Count) { static_cast<T*>(Array)->SetNum(Count); },
				[](void* Array, int32 Index) -> void* { return &(*static_cast<T*>(Array))[Index]; },
				[](void* Dest, const void* Source) { *static_cast<T*>(Dest) = *static_cast<const T*>(Source); },
			};
			Property.Array = &ArrayOps;
		}

		return Property;
	}
};

class UScriptStruct : public UStruct
{
public:
	UScriptStruct(const FString& InName, uint64 InSize, void (*RegisterProperties)(UScriptStruct*))
		: Name(InName), Size(InSize)
	{
		RegisterProperties(this);
	}

	FString Name;
	uint64 Size;
};

#define DECLARE_STRUCT(StructName)                                            \
public:                                                                      \
	using ThisClass = StructName;                                             \
	static UScriptStruct* StaticStruct()                                      \
	{                                                                         \
		static UScriptStruct Type(#StructName, sizeof(StructName),             \
			&StructName::RegisterProperties);                                  \
		return &Type;                                                         \
	}
