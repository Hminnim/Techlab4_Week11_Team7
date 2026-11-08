#pragma once

#include "AnimTypes.h"
#include "AnimCurveTypes.h"
#include "BonePose.h"

namespace ETransformBlendMode
{
	enum Type
	{
		Overwrite,
		Accumulate
	};
}

template<ETransformBlendMode::Type>
void BlendTransform(const FTransform& Source, FBoneTransform& Dest, const float BlendWeight);

template<>
inline void BlendTransform<ETransformBlendMode::Overwrite>(const FTransform& Source, FBoneTransform& Dest, const float BlendWeight)
{
	Dest.Translation = Source.Location * BlendWeight;
	Dest.Scale = Source.Scale * BlendWeight;
	Dest.Rotation = GetBoneRotation(Source) * BlendWeight;	
}

template<>
inline void BlendTransform<ETransformBlendMode::Accumulate>(const FTransform& Source, FBoneTransform& Dest, const float BlendWeight)
{
	Dest.Translation += Source.Location * BlendWeight;
	Dest.Scale += Source.Scale * BlendWeight;

	FQuat SourceQuat = GetBoneRotation(Source);

	const float Bias = SourceQuat.Dot(Dest.Rotation) >= 0.0f ? 1.0f : -1.0f;

	Dest.Rotation += (SourceQuat * (BlendWeight * Bias));
}

class FAnimationRuntime
{
public:
	static void BlendTwoPosesTogether(const FCompactPose& SourcePose1, const FCompactPose& SourcePose2, const FBlendedCurve& SourceCurve1, const FBlendedCurve& SourceCurve2, const float WeightOfPose1, FCompactPose& ResultPose, FBlendedCurve& ResultCurve);
};