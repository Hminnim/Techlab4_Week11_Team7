#pragma once

#include "AnimTypes.h"
#include "AnimCurveTypes.h"
#include "BonePose.h"

class FAnimationRuntime
{
	static void BlendTowPosesTogether(const FCompactPose& SourcePose1, const FCompactPose& SourcePose2, const FBlendedCurve& SourceCurve1, const FBlendedCurve& SourceCurve2, const float WeightOfPose1, FCompactPose& ResultPose, FBlendedCurve& ResultCurve);
};