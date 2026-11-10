#pragma once

#include "Core/EngineString.h"
#include "Core/Misc/Optional.h"
#include "Math/EngineMath.h"
#include "Math/Color.h"
#include "Math/Transform.h"
#include "Math/Color.h"
#include "Enum.h"

class UMaterial;
class UFont;
class UScriptStruct;
struct FArrayPropertyOps;

using FScriptStructAccessor = UScriptStruct* (*)();

enum class EPropertyType { Unknown, Float, Int, String, Bool, Vector, Vector4, Color, LinearColor, Rotator, Transform, Object, Struct, Enum, Array };

template <typename T>
struct TIsPropertyArray : std::false_type {};

template <typename T>
struct TIsPropertyArray<TArray<T>> : std::true_type {};

template <typename T>
constexpr EPropertyType GetPropertyType()
{
    if constexpr (std::is_pointer_v<T> &&
        std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
    {
        return EPropertyType::Object;
    }
    else if constexpr (requires { static_cast<FScriptStructAccessor>(&T::StaticStruct); })
    {
        return EPropertyType::Struct;
    }
    else if constexpr (std::is_enum_v<T> && TEnumInfo<T>::bReflected)
    {
        return EPropertyType::Enum;
    }
    else if constexpr (TIsPropertyArray<T>::value)
    {
        return EPropertyType::Array;
    }
    else
    {
        static_assert(sizeof(T) == 0, "Not Valid Type");
        return EPropertyType::Unknown;
    }
}

#define DEFINE_PROPERTY_TYPE(CppType, EnumValue)                    \
    template <> constexpr EPropertyType GetPropertyType<CppType>()  \
    { return EPropertyType::EnumValue; }

DEFINE_PROPERTY_TYPE(int, Int)
DEFINE_PROPERTY_TYPE(uint32, Int)
DEFINE_PROPERTY_TYPE(float, Float)
DEFINE_PROPERTY_TYPE(bool, Bool)
DEFINE_PROPERTY_TYPE(FString, String)
DEFINE_PROPERTY_TYPE(FVector, Vector)
DEFINE_PROPERTY_TYPE(FVector4, Vector4)
DEFINE_PROPERTY_TYPE(FTransform, Transform)
DEFINE_PROPERTY_TYPE(FRotator, Rotator)
DEFINE_PROPERTY_TYPE(FColor, Color)
DEFINE_PROPERTY_TYPE(FLinearColor, LinearColor)

template <typename T>
inline constexpr bool TSupportsNumericMeta =
    std::is_same_v<T, float> || std::is_same_v<T, int32> || std::is_same_v<T, uint32> ||
    std::is_same_v<T, FVector> || std::is_same_v<T, FVector4> || std::is_same_v<T, FRotator>;

struct FPropertyMeta
{
    TOptional<double> ClampMin;
    TOptional<double> ClampMax;
    TOptional<double> UIMin;
    TOptional<double> UIMax;
    TOptional<double> Delta;
};

// 프로퍼티 지정자 (UE의 UPROPERTY 지정자에 대응)
enum EPropertyFlags : uint32
{
    CPF_None      = 0,
    CPF_Edit      = 1 << 0, // 디테일 패널에 노출
    CPF_EditConst = 1 << 1, // 노출하되 수정 불가
    CPF_Transient = 1 << 2, // 직렬화에서 제외

    CPF_EditAnywhere    = CPF_Edit,
    CPF_VisibleAnywhere = CPF_Edit | CPF_EditConst,
    CPF_Default         = CPF_EditAnywhere,
};

struct FEnumPropertyOps
{
    const FEnumInfo* Info;
    int64 (*Read)(const void* Value);
    void (*Write)(void* Value, int64 Number);
};

struct FProperty
{
    FString Name;
    EPropertyType Type;
    size_t Offset;
    size_t Size;
    UClass* Class = nullptr;
    uint32 Flags = CPF_Default;
    FScriptStructAccessor ResolveScriptStruct = nullptr;
    const FEnumPropertyOps* Enum = nullptr;
    const FArrayPropertyOps* Array = nullptr;
    FPropertyMeta Meta;
    bool bUnsignedInt = false;

    bool HasAnyFlags(uint32 InFlags) const { return (Flags & InFlags) != 0; }
    const UScriptStruct* GetScriptStruct() const
    {
        return ResolveScriptStruct ? ResolveScriptStruct() : nullptr;
    }
};

struct FArrayPropertyOps
{
    const FProperty& (*GetElementProperty)();
    int32 (*Num)(const void* Array);
    void (*Resize)(void* Array, int32 Count);
    void* (*GetElement)(void* Array, int32 Index);
    void (*Copy)(void* Dest, const void* Source);
};
