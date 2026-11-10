#include "EnginePCH.h"
#include "AnimNode_SequencePlayer.h"
#include "Engine/Classes/Animation/AnimInstance.h"

void FAnimNode_SequencePlayer::Initialize(const FAnimationInitializeContext& Context)
{
	CurrentPlayTime = (Sequence && GetEffectivePlayRate() < 0.0f) ? Sequence->GetPlayLength() : 0.0f;
}

void FAnimNode_SequencePlayer::Update(const FAnimationUpdateContext& Context)
{
	if(!Sequence)
	{
		return;
	}

	const float PreviousTime = CurrentPlayTime;
	const float MoveDelta = Context.GetDeltaTime() * GetEffectivePlayRate();
	CurrentPlayTime += MoveDelta;

	UAnimInstance* AnimInstance = Context.GetAnimInstance();

	if (AnimInstance && Sequence->bEnableRootMotion)
	{
		const float Weight = Context.GetFinalBlendWeight() * Context.GetRootMotionWeightModifier();
		const FTransform RootMotion = Sequence->ExtractRootMotion(PreviousTime, MoveDelta, bLoopAnimation);
		AnimInstance->AccumulateRootMotion(RootMotion, Weight);
	}

	float PlayLength = Sequence->GetPlayLength();
	if(bLoopAnimation)
	{
		if(PlayLength > 0.0f)
		{
			while(CurrentPlayTime >= PlayLength)
			{
				CurrentPlayTime -= PlayLength;
			}
			while(CurrentPlayTime < 0.0f)
			{
				CurrentPlayTime += PlayLength;
			}
		}
	}
	else
	{
		CurrentPlayTime  = FMath::Clamp(CurrentPlayTime, 0.0f, PlayLength);
	}
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
	if(!Sequence)
	{
		return;
	}

	FAnimExtractContext ExtractionContext;
	ExtractionContext.CurrentTime = CurrentPlayTime;
	ExtractionContext.bLooping = bLoopAnimation;

	Sequence->GetAnimationPose(Output.Pose, ExtractionContext);
}
