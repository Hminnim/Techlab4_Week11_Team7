#include "EnginePCH.h"
#include "Class.h"

#include "Object.h"

UClass::UClass() = default;

bool UClass::IsChildOf(const UClass* BaseClass) const
{
	if (!BaseClass) return false;
	for (const UClass* c = this; c; c = c->Super)
	{
		if (c == BaseClass) return true;
	}
	return false;
}

UObject* UClass::GetDefaultObject() 
{
	if (!DefaultObject)
	{
		DefaultObject = Constructor(UObject::operator new(ClassSize));
	}
	return DefaultObject;
}

namespace
{
	void CopyFields(const TArray<FProperty>& Fields, const void* Source, void* Dest)
	{
		for (const FProperty& Property : Fields)
		{
			const void* Src = static_cast<const uint8*>(Source) + Property.Offset;
			void* Dst = static_cast<uint8*>(Dest) + Property.Offset;
			switch (Property.Type)
			{
			case EPropertyType::Array:
				Property.Array->Copy(Dst, Src);
				break;
			case EPropertyType::Struct:
				CopyFields(Property.GetScriptStruct()->GetProperties(), Src, Dst);
				break;
			case EPropertyType::String:
				*static_cast<FString*>(Dst) = *static_cast<const FString*>(Src);
				break;
			default:
				memcpy(Dst, Src, Property.Size);
				break;
			}
		}
	}
}

void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass)
{
	if (Src == Dst) return;
	for (const UClass* Class = FromClass; Class; Class = Class->Super)
		CopyFields(Class->GetProperties(), Src, Dst);
}


