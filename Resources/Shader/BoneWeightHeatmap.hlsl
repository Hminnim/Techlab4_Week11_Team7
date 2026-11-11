#pragma pack_matrix(row_major)
#include "Skinning.hlsli"

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

cbuffer BoneWeightConstants : register(b1)
{
    int SelectedBoneIndex;
    int3 BoneWeightPadding;
};

cbuffer WorldConstants : register(b2)
{
    matrix World;
};

// 공통 FVertex와 같은 순서를 유지해 기존 정점 버퍼를 그대로 사용한다.
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float Weight : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT Input, uint VertexID : SV_VertexID)
{
    // 위치 변형은 일반 GPU 스키닝과 동일하다. Weight는 변형 전후에 바뀌지 않는다.
    ApplySkinning(VertexID, Input.Position, Input.Normal, Input.Tangent, false);
    float Weight = 0.0f;
    // 배경 클릭으로 본 선택이 풀려도 Heatmap을 유지하고 Weight 0의 보라색을 표시한다.
    if (SelectedBoneIndex >= 0)
    {
        FSkinWeightInfo Influences = SkinWeights[VertexID];
        [unroll]
        for (uint Index = 0; Index < 4; ++Index)
        {
            if (Influences.InfluenceBones[Index] == SelectedBoneIndex)
                Weight += Influences.InfluenceWeights[Index];
        }
    }

    PS_INPUT Output;
    Output.Position = mul(mul(float4(Input.Position, 1.0f), World), VP);
    Output.Weight = saturate(Weight);
    return Output;
}

float3 WeightToColor(float Weight)
{
    float T = saturate(Weight) * 4.0f;
    if (T < 1.0f) return lerp(float3(0.5f, 0, 1), float3(0, 0, 1), T);
    if (T < 2.0f) return lerp(float3(0, 0, 1), float3(0, 1, 0), T - 1.0f);
    if (T < 3.0f) return lerp(float3(0, 1, 0), float3(1, 1, 0), T - 2.0f);
    return lerp(float3(1, 1, 0), float3(1, 0, 0), T - 3.0f);
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    // 색이 아닌 Weight를 보간한 뒤 변환해야 삼각형 내부에도 전체 색상 단계가 나타난다.
    return float4(WeightToColor(Input.Weight), 1.0f);
}
