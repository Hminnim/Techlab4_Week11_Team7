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
		UClass* ObjClass = nullptr;
		if constexpr (std::is_pointer_v<T> &&
			std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
		{
			ObjClass = std::remove_pointer_t<T>::StaticClass();
		}

		Properties.Add({ InName, GetPropertyType<T>(), InOffset, sizeof(T), ObjClass, InFlags });
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
