#pragma once

#include "Engine/Classes/Animation/AnimationAsset.h"
#include "Engine/Classes/Animation/AnimTypes.h"
#include "AnimSequence.generated.h"

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
	TArray<FBoneAnimationTrack> BoneAnimationTracks;
	float PlayLength = 0.0f;
	FFrameRate FrameRate;
	int32 NumberOfFrames = 0;
	int32 NumberOfKeys = 0;
	FAnimationCurveData CurveData;

	virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const override { return BoneAnimationTracks; }
};

UCLASS()
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_BODY()

public:
	TArray<FAnimNotifyEvent> Notifies;

	UAnimDataModel* GetDataModel() const { return DataModel; }
	virtual float GetPlayLength() const override { return DataModel ? DataModel->PlayLength : 0.0f; }

protected:
	UAnimDataModel* DataModel = nullptr;
};

UCLASS()
class UAnimSequence : public UAnimSequenceBase
{
	GENERATED_BODY()
};
