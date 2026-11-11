#ifndef SKINNING_HLSLI
#define SKINNING_HLSLI

cbuffer SkinningConstants : register(b7)
{
    uint UseGPUSkinning;
    uint3 SkinningPadding;
};

struct FSkinWeightInfo
{
    int4 InfluenceBones;
    float4 InfluenceWeights;
};

// FMatrix를 전치하지 않고 업로드한다. 명시적인 행으로 CPU 메모리 배치를 맞춘다.
struct FSkinningMatrix
{
    float4 Row0;
    float4 Row1;
    float4 Row2;
    float4 Row3;
};

StructuredBuffer<FSkinWeightInfo> SkinWeights : register(t14);
StructuredBuffer<FSkinningMatrix> SkinningMatrices : register(t15);

float3 SkinSafeNormal(float3 Value, float3 Fallback)
{
    float LengthSquared = dot(Value, Value);
    if (LengthSquared < 1.0e-8f) return Fallback;
    return Value * rsqrt(LengthSquared);
}

void ApplySkinning(uint VertexID, inout float3 Position,
    inout float3 Normal, inout float4 Tangent, bool TransformDirections)
{
    // CPU 결과와 StaticMesh는 입력 정점을 그대로 사용한다.
    [branch]
    if (UseGPUSkinning == 0) return;

    FSkinWeightInfo Influences = SkinWeights[VertexID];
    float4x4 Matrix = (float4x4)0;
    bool HasInfluence = false;
    [unroll]
    for (uint Index = 0; Index < 4; ++Index)
    {
        float Weight = Influences.InfluenceWeights[Index];
        if (Weight == 0.0f) continue;
        FSkinningMatrix Bone = SkinningMatrices[Influences.InfluenceBones[Index]];
        Matrix += float4x4(Bone.Row0, Bone.Row1, Bone.Row2, Bone.Row3) * Weight;
        HasInfluence = true;
    }
    if (!HasInfluence) return;

    Position = mul(float4(Position, 1.0f), Matrix).xyz;
    // 스텐실 마스크는 위치만 필요하다. 노멀/탄젠트 변환은 확장 패스에서 수행한다.
    if (!TransformDirections) return;
    float3 XAxis = Matrix[0].xyz;
    float3 YAxis = Matrix[1].xyz;
    float3 ZAxis = Matrix[2].xyz;
    float3 CrossYZ = cross(YAxis, ZAxis);
    float Handedness = dot(XAxis, CrossYZ) < 0.0f ? -1.0f : 1.0f;

    // CPU 경로와 동일하게 비균일/음수 스케일의 노멀과 탄젠트 부호를 반영한다.
    float3 SkinnedNormal = (CrossYZ * Normal.x + cross(ZAxis, XAxis) * Normal.y
        + cross(XAxis, YAxis) * Normal.z) * Handedness;
    Normal = SkinSafeNormal(SkinnedNormal, SkinSafeNormal(Normal, float3(0, 0, 0)));

    float3 SkinnedTangent = mul(float4(Tangent.xyz, 0.0f), Matrix).xyz;
    SkinnedTangent -= Normal * dot(SkinnedTangent, Normal);
    if (dot(SkinnedTangent, SkinnedTangent) < 1.0e-8f)
    {
        float3 Axis = abs(Normal.z) < 0.9f ? float3(0, 0, 1) : float3(0, 1, 0);
        SkinnedTangent = cross(Axis, Normal);
    }
    Tangent = float4(SkinSafeNormal(SkinnedTangent, float3(0, 0, 0)), Tangent.w * Handedness);
}

void ApplySkinning(uint VertexID, inout float3 Position,
    inout float3 Normal, inout float4 Tangent)
{
    ApplySkinning(VertexID, Position, Normal, Tangent, true);
}

#endif
