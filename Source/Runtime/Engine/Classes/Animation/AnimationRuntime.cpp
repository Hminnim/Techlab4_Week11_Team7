#include "EnginePCH.h"
#include "AnimationRuntime.h"
#include "AnimSequence.h"
#include "BonePose.h"

template <ETransformBlendMode::Type BlendMode>
void BlendPose(const FCompactPose& SourcePose, TArray<FBoneTransform>& ResultPose, const float BlendWeight)
{
	for(int32 i = 0; i < SourcePose.GetNumBones(); ++i)
	{
		BlendTransform<BlendMode>(SourcePose[i], ResultPose[i], BlendWeight);
	}

}

template<>
void BlendPose<ETransformBlendMode::Overwrite>(const FCompactPose& SourcePose, TArray<FBoneTransform>& ResultPose, const float BlendWeight)
{
	for(int32 i = 0; i < SourcePose.GetNumBones(); ++i)
	{
		BlendTransform<ETransformBlendMode::Overwrite>(SourcePose[i], ResultPose[i], BlendWeight);
	}
}

template<>
void BlendPose<ETransformBlendMode::Accumulate>(const FCompactPose& SourcePose, TArray<FBoneTransform>& ResultPose, const float BlendWeight)
{
	for(int32 i = 0; i < SourcePose.GetNumBones(); ++i)
	{
		BlendTransform<ETransformBlendMode::Accumulate>(SourcePose[i], ResultPose[i], BlendWeight);
	}
}

void FAnimationRuntime::BlendTwoPosesTogether(const FCompactPose& SourcePose1, const FCompactPose& SourcePose2, const FBlendedCurve& SourceCurve1, const FBlendedCurve& SourceCurve2, const float WeightOfPoseOne, FCompactPose& ResultPose, FBlendedCurve& ResultCurve)
{
	ResultPose.SetRefSkeleton(SourcePose1.GetRefSkeleton());
	TArray<FBoneTransform> TempAtoms;
	TempAtoms.SetNum(SourcePose1.GetNumBones());

	const float WeightOfPoseTwo = 1.0f - WeightOfPoseOne;

	BlendPose<ETransformBlendMode::Overwrite>(SourcePose1, TempAtoms, WeightOfPoseOne);
	BlendPose<ETransformBlendMode::Accumulate>(SourcePose2, TempAtoms, WeightOfPoseTwo);

	for(int32 BoneIndex = 0; BoneIndex < SourcePose1.GetNumBones(); ++BoneIndex)
	{
		TempAtoms[BoneIndex].NormalizeRotation();
		ResultPose[BoneIndex] = TempAtoms[BoneIndex].ToTransform();
	}
}
