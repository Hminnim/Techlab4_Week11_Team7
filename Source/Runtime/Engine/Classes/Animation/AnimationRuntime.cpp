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

void FAnimationRuntime::BlendPosesTogether(const TArray<FCompactPose>& SourcePoses, const TArray<FBlendedCurve>& SourceCurves, const TArray<float>& SourceWeights, FCompactPose& ResultPose, FBlendedCurve& ResultCurve)
{
	const int32 NumPoses = SourcePoses.Num();
	if(NumPoses == 0)
	{
		ResultPose.ResetToRefPose();
		return;
	}

	ResultPose.SetRefSkeleton(SourcePoses[0].GetRefSkeleton());
	
	if(NumPoses == 1)
	{
		ResultPose = SourcePoses[0];
		if(SourceCurves.Num() > 0) ResultCurve = SourceCurves[0];
		return;
	}
	const int32 NumBones = ResultPose.GetNumBones();
	TArray<FBoneTransform> TempAtoms;
	TempAtoms.SetNum(NumBones);
	
	BlendPose<ETransformBlendMode::Overwrite>(SourcePoses[0], TempAtoms, SourceWeights[0]);
	
	for(int32 PoseIndex = 1; PoseIndex < NumPoses; ++PoseIndex)
	{
		BlendPose<ETransformBlendMode::Accumulate>(SourcePoses[PoseIndex], TempAtoms, SourceWeights[PoseIndex]);
	}
	
	for(int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		TempAtoms[BoneIndex].NormalizeRotation();
		ResultPose[BoneIndex] = TempAtoms[BoneIndex].ToTransform();
	}
}

ETypeAdvanceAnim FAnimationRuntime::AdvanceTime(const bool bAllowLooping, const float MoveDelta, float& InOutTime, const float EndTIme)
{
	float NewTime = InOutTime + MoveDelta;

	if(NewTime < 0.0f || NewTime >EndTIme)
	{
		if(bAllowLooping)
		{
			if(EndTIme != 0.0f)
			{
				NewTime = fmod(NewTime, EndTIme);

				if(NewTime < 0.0f)
				{
					NewTime += EndTIme;
				}
			}
			else
			{
				NewTime = 0.0f;
			}

			InOutTime = NewTime;
			return ETypeAdvanceAnim::Looped;
		}
		else
		{
			InOutTime = FMath::Clamp(NewTime, 0.0f, EndTIme);
			return ETypeAdvanceAnim::Finished;
		}
	}
	
	InOutTime = NewTime;
	return ETypeAdvanceAnim::Default;
}
