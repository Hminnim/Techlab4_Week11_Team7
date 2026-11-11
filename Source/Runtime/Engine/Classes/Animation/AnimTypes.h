#pragma once

#include "Core/Types.h"
#include "Core/NameTypes.h"
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "MovieScene/MovieSceneFrameTime.h"
#include "Math/Transform.h"

using FVector3f = FVector;
using FQuat4f = FQuat;

struct FAnimNotifyEvent
{
	float TriggerTime = 0.0f;
	float Duration = 0.0f; // 0 이면 한 번, 0 보다 크면 구간(NotifyState)
	FName NotifyName;

	float GetTriggerTime() const
	{
		return TriggerTime;
	}
	 
	float GetEndTriggerTime() const
	{
		return (Duration > 0.0f) ? (TriggerTime + Duration) : TriggerTime;
	}

	bool IsNotifyState() const
	{
		return Duration > 0.0f;
	}
};

struct FRawAnimSequenceTrack
{
	TArray<FVector3f> PosKeys;   // 위치 키프레임
	TArray<FQuat4f> RotKeys;     // 회전 키프레임 (Quaternion)
	TArray<FVector3f> ScaleKeys; // 스케일 키프레임
};

struct FBoneAnimationTrack
{
	FName Name;                          // Bone 이름
	FRawAnimSequenceTrack InternalTrack; // 실제 애니메이션 데이터
};

// 임포터가 읽은 애니메이션 원본.
// UAnimDataModel을 채우는데 사용.
struct FAnimSequenceImportData
{
	FString Name;
	FFrameRate FrameRate = FFrameRate(30, 1);
	int32 NumberOfFrames = 0;					// 키 수 = NumberOfFrames + 1
	TArray<FBoneAnimationTrack> Tracks;			// 본마다 한개
};

struct FAnimationCurveData
{
};

// 그래프를 한번 갱신하는 동안 재생 노드들이 추출한 루트 모션을 모은다.
struct FRootMotionMovementParams
{
	bool bHasRootMotion = false;

	void AccumulateWithBlend(const FTransform& InTransform, float InBlendWeight)
	{
		RootMotionTransform.Location += InTransform.Location * InBlendWeight;
		bHasRootMotion = true;
	}

	void Clear()
	{
		bHasRootMotion = false;
		RootMotionTransform = FTransform();
	}

	const FTransform& GetRootMotionTransform() const { return RootMotionTransform; }

private:
	FTransform RootMotionTransform;
};