#include "EnginePCH.h"
#include "Struct.h"

const FProperty* UStruct::FindProperty(const FString& InName) const
{
	for (const UStruct* Type = this; Type; Type = Type->GetSuperStruct())
	{
		for (const FProperty& Property : Type->GetProperties())
		{
			if (Property.Name == InName)
				return &Property;
		}
	}
	return nullptr;
}
