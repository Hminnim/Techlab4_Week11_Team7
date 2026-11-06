#pragma once

#include "Core/Types.h"
#include "Core/NameTypes.h"
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "MovieScene/MovieSceneFrameTime.h"

using FVector3f = FVector;
using FQuat4f = FQuat;

struct FAnimNotifyEvent
{
	float TriggerTime = 0.0f;
	float Duration = 0.0f; // 0 이면 한 번, 0 보다 크면 구간(NotifyState)
	FName NotifyName;
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

struct FAnimationCurveData
{
};
