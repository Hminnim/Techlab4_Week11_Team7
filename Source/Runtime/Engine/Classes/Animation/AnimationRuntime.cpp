#include "EnginePCH.h"
#include "AnimationRuntime.h"

void FAnimationRuntime::BlendTowPosesTogether(const FCompactPose& SourcePose1, const FCompactPose& SourcePose2, const FBlendedCurve& SourceCurve1, const FBlendedCurve& SourceCurve2, const float WeightOfPoseOne, FCompactPose& ResultPose, FBlendedCurve& ResultCurve)
{
	const float WeightOfPoseTwo = 1.0f - WeightOfPoseOne;
}
