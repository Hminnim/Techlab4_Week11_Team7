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

	const float DeltaMove = Context.GetDeltaTime() * GetEffectivePlayRate();
	FAnimNotifyQueue* NotifyQueue = Context.GetAnimInstance() ? &Context.GetAnimInstance()->NotifyQueue : nullptr;
	Sequence->TickAssetPlayer(CurrentPlayTime, DeltaMove, bLoopAnimation, NotifyQueue);
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
