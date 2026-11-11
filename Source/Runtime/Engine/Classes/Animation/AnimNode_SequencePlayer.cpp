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

	const float MoveDelta = Context.GetDeltaTime() * GetEffectivePlayRate();
	UAnimInstance* AnimInstance = Context.GetAnimInstance();

	FAnimNotifyQueue* NotifyQueue = AnimInstance ? &Context.GetAnimInstance()->NotifyQueue : nullptr;

	if (AnimInstance && Sequence->bEnableRootMotion)
	{
		const float Weight = Context.GetFinalBlendWeight() * Context.GetRootMotionWeightModifier();
		const FTransform RootMotion = Sequence->ExtractRootMotion(CurrentPlayTime, MoveDelta, bLoopAnimation);
		AnimInstance->AccumulateRootMotion(RootMotion, Weight);
	}

	Sequence->TickAssetPlayer(CurrentPlayTime, MoveDelta, bLoopAnimation, NotifyQueue);
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
