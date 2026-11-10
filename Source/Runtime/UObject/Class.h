#pragma once

#include "Struct.h"

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

	template<class T>
	T* GetDefaultObject()
	{
		return CastChecked<T>(GetDefaultObject());
	}
private:
	const UStruct* GetSuperStruct() const override { return Super; }

	mutable int32 ClassUnique = 0;
	friend FName MakeUniqueObjectName(UObject*, const UClass*, FName);
};

void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass);
