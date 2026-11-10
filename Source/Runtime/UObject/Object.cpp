#include "EnginePCH.h"
#include "Object.h"

#include <limits>

#include "Core/EngineStatics.h"
#include "UObject/Class.h"

#include "Serialization/TypeSerializer.h"
#include "Serialization/MathSerialization.h"
#include "Serialization/Formatter/BinaryArchiveFormatter.h"
#include "Asset/AssetManager.h" //??? 의존성 제거 필요



TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	ObjectUUID = FEngineStatics::GetUUID();
	InternalIndex = GUObjectArray.Num();
	GUObjectArray.Add(this);
}

UObject::UObject(bool bRegister)
{
	bIsRegistered = bRegister;
}

UObject::~UObject()
{
	if (bIsRegistered)
	{
		if (ClassPrivate)
		{
			UnhashObject(this, ClassPrivate);
		}
		int32 LastIndex = GUObjectArray.Num() - 1;
		if (InternalIndex != LastIndex)
		{
			UObject* MovedObject = GUObjectArray[LastIndex];
			GUObjectArray[InternalIndex] = MovedObject;
			MovedObject->InternalIndex = InternalIndex; // 옮겨간 오브젝트 인덱스 갱신
		}
		GUObjectArray.RemoveLast();
	}
}

UClass* UObject::StaticClass()
{
	static UClass c;
	static bool bIsInit = false;
	if (!bIsInit)
	{
		c.Name = "Object";
		c.Super = nullptr;
		c.ClassSize = sizeof(UObject);
		c.Constructor = [](void* Memory) -> UObject*
			{
				return ::new (Memory) UObject();
			};
		bIsInit = true;
	}
	return &c;
}

bool UObject::IsA(const UClass* Class) const
{
	const UClass* ThisClass = GetClass();
	return ThisClass && ThisClass->IsChildOf(Class);
}

namespace
{
	void SerializeJsonFields(json& Handle, void* Container, const TArray<FProperty>& Fields, bool bIsLoading);
	void SerializeArchiveFields(FStructuredArchive::FRecord Record, void* Container, const TArray<FProperty>& Fields, bool bIsLoading);

	bool SerializeJsonValue(json& Field, void* Ptr, const FProperty& Property, bool bIsLoading)
	{
		switch (Property.Type)
		{
		case EPropertyType::Float:
		{
			float& Value = *static_cast<float*>(Ptr);
			if (bIsLoading) Value = Field.get<float>();
			else Field = Value;
			break;
		}
		case EPropertyType::Int:
		{
			int32& Value = *static_cast<int32*>(Ptr);
			if (bIsLoading) Value = Field.get<int32>();
			else Field = Value;
			break;
		}
		case EPropertyType::String:
		{
			FString& Value = *static_cast<FString*>(Ptr);
			if (bIsLoading) Value = Field.get<std::string>();
			else Field = Value;
			break;
		}
		case EPropertyType::Bool:
		{
			bool& Value = *static_cast<bool*>(Ptr);
			if (bIsLoading) Value = Field.get<bool>();
			else Field = Value;
			break;
		}
		case EPropertyType::Vector:
		{
			FVector& Value = *static_cast<FVector*>(Ptr);
			if (bIsLoading) Value = Field.get<FVector>();
			else Field = Value;
			break;
		}
		case EPropertyType::Vector4:
		{
			FVector4& Value = *static_cast<FVector4*>(Ptr);
			if (bIsLoading) Value = Field.get<FVector4>();
			else Field = Value;
			break;
		}
		case EPropertyType::Color:
		{
			// FColor(4바이트)는 RGBA를 묶은 uint32로 저장한다
			FColor& Value = *static_cast<FColor*>(Ptr);
			if (bIsLoading) Value.DWColor() = Field.get<uint32>();
			else Field = Value.DWColor();
			break;
		}
		case EPropertyType::LinearColor:
		{
			FVector4& Value = *static_cast<FVector4*>(Ptr);
			if (bIsLoading) Value = Field.get<FVector4>();
			else Field = Value;
			break;
		}
		case EPropertyType::Transform:
		{
			FTransform& Value = *static_cast<FTransform*>(Ptr);
			if (bIsLoading) Value = Field.get<FTransform>();
			else Field = Value;
			break;
		}
		case EPropertyType::Struct:
		{
			if (!bIsLoading) Field = json::object();
			if (Field.is_object())
				SerializeJsonFields(Field, Ptr, Property.GetScriptStruct()->GetProperties(), bIsLoading);
			break;
		}
		case EPropertyType::Enum:
			if (bIsLoading) Property.Enum->Write(Ptr, Field.get<int64>());
			else Field = Property.Enum->Read(Ptr);
			break;
		case EPropertyType::Array:
		{
			const FArrayPropertyOps& Array = *Property.Array;
			int32 Count = Array.Num(Ptr);
			if (bIsLoading)
			{
				if (!Field.is_array() || Field.size() > static_cast<size_t>(std::numeric_limits<int32>::max()))
					break;

				Count = static_cast<int32>(Field.size());
				Array.Resize(Ptr, Count);
			}
			else
			{
				Field = json::array();
			}

			const FProperty& Element = Array.GetElementProperty();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				json& Entry = bIsLoading ? Field[Index] : Field.emplace_back();
				SerializeJsonValue(Entry, Array.GetElement(Ptr, Index), Element, bIsLoading);
			}
			break;
		}
		case EPropertyType::Object:
		{
			UObject*& Value = *static_cast<UObject**>(Ptr);

			if (bIsLoading)
			{
				// null로 저장된 건 "되찾을 수 없는 값"이었으므로 기본값을 유지한다
				if (Field.is_null())
				{
					break;
				}

				const FString AssetPath = Field.get<FString>();
				URenderAsset* Asset = nullptr;

				if (Property.Class && Property.Class->IsChildOf(UStaticMesh::StaticClass()))
				{
					Asset = UAssetManager::GetAssetByPath<UStaticMesh>(AssetPath);
				}
				else
				{
					Asset = UAssetManager::GetAssetByPath<URenderAsset>(AssetPath);
				}

				// 못 찾으면 생성자가 넣어둔 기본값을 그대로 둔다.
				// (파일 하나가 없어져도 씬은 열리고, 액터가 사라지지 않고 기본 모양으로 보인다)
				if (!Asset || (Property.Class && !Asset->IsA(Property.Class)))
				{
					HTR_LOG(Warning, "Load: asset '{}' not found for {}, keeping default", AssetPath, Property.Name);
					break;
				}

				Value = Asset;
			}
			else
			{
				// 경로가 없는 에셋(런타임에 만든 인스턴스 등)은 되찾을 방법이 없으니 null로 쓴다
				URenderAsset* Asset = Cast<URenderAsset>(Value);
				if (Asset && !Asset->GetPath().empty())
					Field = Asset->GetPath();
				else
					Field = nullptr;
			}
			break;
		}
		default:
			return false;
		}
		return true;
	}

	void SerializeJsonFields(json& Handle, void* Container, const TArray<FProperty>& Fields, bool bIsLoading)
	{
		for (const FProperty& Property : Fields)
		{
			if (Property.HasAnyFlags(CPF_Transient))
				continue;

			void* Ptr = static_cast<uint8*>(Container) + Property.Offset;
			if (bIsLoading)
			{
				auto Field = Handle.find(Property.Name);
				if (Field != Handle.end())
					SerializeJsonValue(*Field, Ptr, Property, true);
			}
			else
			{
				json Field;
				if (SerializeJsonValue(Field, Ptr, Property, false))
					Handle[Property.Name] = std::move(Field);
			}
		}
	}

	void SerializeArchiveValue(FStructuredArchive::FSlot Value, void* Ptr, const FProperty& Property, bool bIsLoading)
	{
		switch (Property.Type)
		{
		case EPropertyType::Float:     Value << *static_cast<float*>(Ptr); break;
		case EPropertyType::Int:       Value << *static_cast<int32*>(Ptr); break;
		case EPropertyType::String:    Value << *static_cast<FString*>(Ptr); break;
		case EPropertyType::Bool:      Value << *static_cast<bool*>(Ptr); break;
		case EPropertyType::Vector:    Value << *static_cast<FVector*>(Ptr); break;
		case EPropertyType::Vector4:
		case EPropertyType::LinearColor: Value << *static_cast<FVector4*>(Ptr); break;
		case EPropertyType::Color:     Value << *static_cast<FColor*>(Ptr); break;
		case EPropertyType::Rotator:   Value << *static_cast<FRotator*>(Ptr); break;
		case EPropertyType::Transform: Value << *static_cast<FTransform*>(Ptr); break;
		case EPropertyType::Struct:
			SerializeArchiveFields(Value.EnterRecord(), Ptr, Property.GetScriptStruct()->GetProperties(), bIsLoading);
			break;
		case EPropertyType::Enum:
		{
			int64 Number = Property.Enum->Read(Ptr);
			Value << Number;
			if (bIsLoading) Property.Enum->Write(Ptr, Number);
			break;
		}
		case EPropertyType::Array:
		{
			const FArrayPropertyOps& Array = *Property.Array;
			int32 Count = bIsLoading ? 0 : Array.Num(Ptr);
			FStructuredArchive::FArray Entries = Value.EnterArray(Count);
			FArchive& Archive = Value.GetUnderlyingArchive();
			if (Count < 0 || Archive.IsError())
			{
				Archive.SetError();
				break;
			}
			if (bIsLoading)
				Array.Resize(Ptr, Count);

			const FProperty& Element = Array.GetElementProperty();
			for (int32 Index = 0; Index < Count; ++Index)
				SerializeArchiveValue(Entries.EnterElement(), Array.GetElement(Ptr, Index), Element, bIsLoading);
			break;
		}
		case EPropertyType::Object:
		{
			if(Property.Class == UMaterial::StaticClass())
			{
				UMaterial* Mat = *reinterpret_cast<UMaterial**>(Ptr);
				UMaterial::SerializeMaterialReference(Value, Mat);
				if(bIsLoading)
				{
					*reinterpret_cast<UMaterial**>(Ptr) = Mat;
				}
				break;
			}

			UObject*& Ref = *static_cast<UObject**>(Ptr);
			UObject* Previous = Ref;
			Value << Ref;

			// 불러온 객체가 프로퍼티가 기대하는 클래스가 아니면 되돌린다 (기존 Asset->IsA(Property.Class) 검사)
			if (bIsLoading && Ref && Property.Class && !Ref->IsA(Property.Class))
			{
				HTR_LOG(Warning, "Load: '{}' is not a {}, keeping default", Property.Name, Property.Class->Name);
				Ref = Previous;
			}
			break;
		}
		default:
			// 슬롯을 열었으면 반드시 값을 써야 한다. 모르는 타입은 저장 시 null로 둔다.
			HTR_LOG(Warning, "Serialize: unsupported property type for '{}'", Property.Name);
			{ UObject* Null = nullptr; Value << Null; }
			break;
		}
	}

	void SerializeArchiveFields(FStructuredArchive::FRecord Record, void* Container, const TArray<FProperty>& Fields, bool bIsLoading)
	{
		for (const FProperty& Property : Fields)
		{
			if (Property.HasAnyFlags(CPF_Transient))
				continue;

			TOptional<FStructuredArchive::FSlot> Slot = Record.TryEnterField(Property.Name.c_str(), true);
			if (!Slot.IsSet()) continue;

			void* Ptr = static_cast<uint8*>(Container) + Property.Offset;
			SerializeArchiveValue(Slot.GetValue(), Ptr, Property, bIsLoading);
		}
	}
}

void UObject::Serialize(json& Handle, bool bIsLoading)
{
	for (UClass* Class = GetClass(); Class; Class = Class->Super)
		SerializeJsonFields(Handle, this, Class->GetProperties(), bIsLoading);
}

void UObject::Serialize(FArchive& Ar)
{
	FBinaryArchiveFormatter Formatter(Ar);
	FStructuredArchive StructuredAr(Formatter);
	Serialize(StructuredAr.Open().EnterRecord());
	StructuredAr.Close();
}

void UObject::Serialize(FStructuredArchive::FRecord Record)
{
	SerializeScriptProperties(Record.EnterField("Properties"));
}

void UObject::SerializeScriptProperties(FStructuredArchive::FSlot Slot)
{
	FStructuredArchive::FRecord Properties = Slot.EnterRecord();
	const bool bIsLoading = Properties.GetUnderlyingArchive().IsLoading();
	for (UClass* Class = GetClass(); Class; Class = Class->Super)
		SerializeArchiveFields(Properties, this, Class->GetProperties(), bIsLoading);
}



