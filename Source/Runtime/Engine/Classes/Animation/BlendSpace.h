#pragma once

#include "AnimationAsset.h"
#include "AnimSequence.h"
#include "AnimCurveTypes.h"
#include "BlendSpace.generated.h"

struct FBlendSample
{
	UAnimSequence* Animation = nullptr;
	FVector SamepleVector = FVector::ZeroVector;

	float GetSamplePlayLength() const
	{
		return Animation ? Animation->GetPlayLength() : 0.0f;
	}
};

UCLASS()
class UBlendSpace : public UAnimationAsset
{
	GENERATED_BODY();

public:
	TArray<FBlendSample> SampleData;

	int32 AddSample(const FVector& SampleValue);
	int32 AddSample(UAnimSequence* Animation, const FVector& SampleValue);

	const FBlendSample& GetBlendSample(int32 SampleIndex) const { return SampleData[SampleIndex]; }
	int32 GetNumberOfBlendSamples() const { return SampleData.Num(); }

	bool GetSamplesFromBlendInput(const FVector& BlendInput, TArray<FBlendSampleData>& OutSampleDataList) const;
	float GetAnimationLengthFromSampleData(const TArray<FBlendSampleData>& SampleDataList) const;

	void GetAnimationPose(TArray<FBlendSampleData>& BlendSampleDataCache, FCompactPose& OutPose, FBlendedCurve& OutCurve) const;
};
