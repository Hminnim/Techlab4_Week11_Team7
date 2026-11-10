#pragma once

#include "Function.h"

using ClassConstructor = UObject * (*)(void* Memory);

class UClass : public UStruct
{
public:
	UClass();

	FString Name;
	UClass* Super = nullptr;
	uint64 ClassSize = 0;
	ClassConstructor Constructor = nullptr;

	UObject* DefaultObject = nullptr;

	bool IsChildOf(const UClass* BaseClass) const;
	UObject* GetDefaultObject();
	const UFunction* FindFunction(const FString& InName) const;

	template <auto Method>
	void AddFunction(const FString& InName, uint32 InFlags)
	{
		Functions.Add(UFunction::Create<Method>(InName, InFlags, this));
	}

	template<class T>
	T* GetDefaultObject()
	{
		return CastChecked<T>(GetDefaultObject());
	}
private:
	const UStruct* GetSuperStruct() const override { return Super; }

	TArray<UFunction> Functions;
	mutable int32 ClassUnique = 0;
	friend FName MakeUniqueObjectName(UObject*, const UClass*, FName);
};

void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass);
