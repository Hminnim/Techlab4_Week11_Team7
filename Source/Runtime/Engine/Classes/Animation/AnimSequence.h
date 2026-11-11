#pragma once

#include "Engine/Classes/Animation/AnimationAsset.h"
#include "Engine/Classes/Animation/AnimTypes.h"
#include "AnimNotifyQueue.h"
#include "AnimSequence.generated.h"

typedef TArray<FTransform> FTransformArrayA2;

struct FCompactPose;

enum ETypeAdvanceAnim
{
	Default,
	Finished,
	Looped
};

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

	UAnimDataModel* CreateDataModel();
	UAnimDataModel* GetDataModel() const { return DataModel; }

	// 시간 t의 포즈를 OutPose에 채워준다.
	virtual void GetAnimationPose(FCompactPose& OutPose, const FAnimExtractContext& ExtractionContext) const = 0;
	void OnDataModelDestroyed(const UAnimDataModel* Model);

	virtual float GetPlayLength() const override { return DataModel ? DataModel->GetPlayLength() : 0.0f; }
	inline FFrameRate GetSamplingFrameRate() const { return DataModel ? DataModel->GetFrameRate() :  FFrameRate(30, 1); }
	inline int32 GetNumberOfSampledKeys() const { return DataModel ? DataModel->GetNumberOfKeys() : 0; }

	void GetAnimNotifies(const float& StartTime, const float& DeltaTime, FAnimNotifyContext& NotifyContext) const;
	virtual void GetAnimNotifiesFromDeltaPosition(const float& PreviousPosition, const float& CurrentPosition, FAnimNotifyContext& NotifyContext) const;

	void TickAssetPlayer(float& InOutTime, float DeltaMove, bool bLooping, FAnimNotifyQueue* OutNotifyQueue) const;

public:
	TArray<FAnimNotifyEvent> Notifies;
	float RateScale = 1.0f;

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
	FTransform ExtractRootMotion(float StartTime, float DeltaTime, bool bAllowLooping) const;

public:
	// Root Motion을 키면 포즈의 root bone 위치를 바인드 포즈에 고정하고, 움직인 양은 ExtractRootMotion으로 추출한다.
	bool bEnableRootMotion = false;

private:
	// Root Motion 을 추출할 트랙. "root" 트랙이 있을 시 해당 트랙, 없으면 가장 위 0번 트랙.
	const FBoneAnimationTrack* FindRootMotionTrack() const;
	FVector SampleTrackLocation(const FBoneAnimationTrack& Track, float Time) const;
	
	// GetBonePose에서 트랙별 본 인덱스 캐싱에 활용
	mutable TArray<int32> TrackBoneIndexHints;
};
