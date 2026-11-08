#pragma once

#include "Engine/Classes/Animation/AnimationAsset.h"
#include "Engine/Classes/Animation/AnimTypes.h"
#include "AnimSequence.generated.h"

typedef TArray<FTransform> FTransformArrayA2;

struct FCompactPose;

class IAnimationDataModel
{
public:
	virtual ~IAnimationDataModel() = default;
	virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const = 0;
};

UCLASS()
class UAnimDataModel : public UObject, public IAnimationDataModel
{
	GENERATED_BODY()

public:
	~UAnimDataModel() override;

	TArray<FBoneAnimationTrack> BoneAnimationTracks;
	FAnimationCurveData CurveData;

	virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const override { return BoneAnimationTracks; }
	void SetFrameRateAndNumberOfFrames(const FFrameRate& InFrameRate, int32 InNumberOfFrames);

	inline float GetPlayLength() const { return PlayLength; }
	inline FFrameRate GetFrameRate() const { return FrameRate; }
	inline int32 GetNumberOfFrames() const { return NumberOfFrames; }
	inline int32 GetNumberOfKeys() const { return NumberOfKeys; }

private:
	float PlayLength = 0.0f;
	FFrameRate FrameRate = FFrameRate(30, 1);
	int32 NumberOfFrames = 0;
	int32 NumberOfKeys = 0;
};

UCLASS()
class UAnimSequenceBase : public UAnimationAsset
{      
	GENERATED_BODY()

public:
	~UAnimSequenceBase() override;

	TArray<FAnimNotifyEvent> Notifies;

	UAnimDataModel* CreateDataModel();
	UAnimDataModel* GetDataModel() const { return DataModel; }

	// 시간 t의 포즈를 OutPose에 채워준다.
	virtual void GetAnimationPose(FCompactPose& OutPose, const FAnimExtractContext& ExtractionContext) const = 0;
	void OnDataModelDestroyed(const UAnimDataModel* Model);

	virtual float GetPlayLength() const override { return DataModel ? DataModel->GetPlayLength() : 0.0f; }
	inline FFrameRate GetSamplingFrameRate() const { return DataModel ? DataModel->GetFrameRate() :  FFrameRate(30, 1); }
	inline int32 GetNumberOfSampledKeys() const { return DataModel ? DataModel->GetNumberOfKeys() : 0; }

protected:
	UAnimDataModel* DataModel = nullptr;
};

UCLASS()
class UAnimSequence : public UAnimSequenceBase
{
	GENERATED_BODY()

public:
	virtual void GetAnimationPose(FCompactPose& OutPose, const FAnimExtractContext& ExtractionContext) const override;
	void GetBonePose(FCompactPose& OutPose, const FAnimExtractContext& ExtractionContext) const;
};
