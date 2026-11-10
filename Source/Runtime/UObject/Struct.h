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
	void AddProperty(const FString& InName, uint64 InOffset, uint32 InFlags = CPF_Default)
	{
		constexpr EPropertyType Type = GetPropertyType<T>();
		FProperty Property{ InName, Type, InOffset, sizeof(T), nullptr, InFlags };
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

		Properties.Add(std::move(Property));
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset, EPropertyType InType, uint32 InFlags = CPF_Default)
	{
		Properties.Add({ InName, InType, InOffset, sizeof(T), nullptr, InFlags });
	}

protected:
	UStruct() : UObject(false) {}
	virtual const UStruct* GetSuperStruct() const { return nullptr; }
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
