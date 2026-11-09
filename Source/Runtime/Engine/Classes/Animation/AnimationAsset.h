#pragma once

#include "Asset/RenderAsset.h"
#include "AnimationAsset.generated.h"

class UAnimSequence;

struct FBlendSampleData
{
	UAnimSequence* Animation = nullptr;
	int32 SampleDataIndex = 0;
	float TotalWeight = 0.0f;
	float WeightRate = 0.0f;
	float Time = 0.0f;

	void AddWeight(float Weight)
	{
		TotalWeight += Weight;
	}

	float GetClampedWeight() const
	{
		return FMath::Clamp<float>(TotalWeight, 0.0f, 1.0f);
	}
};

// 시퀀스에서 포즈를 뽑을 때 넘기는 조건
struct FAnimExtractContext
{
	float CurrentTime = 0.0f;	// 샘플링할 에셋 시간 (초)
	bool bLooping = false;
};

UCLASS()
class UAnimationAsset : public URenderAsset
{
	GENERATED_BODY();

public:
	virtual float GetPlayLength() const { return 0.0f; }
};
