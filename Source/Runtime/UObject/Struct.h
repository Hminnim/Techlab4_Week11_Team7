#pragma once

#include "Object.h"
#include "Property.h"

class UStruct : public UObject
{
public:
	TArray<FProperty> Properties;

	const TArray<FProperty>& GetProperties() const { return Properties; }
	const FProperty* FindProperty(const FString& InName) const;

protected:
	UStruct() : UObject(false) {}
	virtual const UStruct* GetSuperStruct() const { return nullptr; }
};
