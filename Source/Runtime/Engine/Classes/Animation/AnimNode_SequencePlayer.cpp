#include "EnginePCH.h"
#include "AnimNode_SequencePlayer.h"

void FAnimNode_SequencePlayer::Initialize(const FAnimationInitializeContext& Context)
{
	CurrentPlayTime = 0.0f;
}

void FAnimNode_SequencePlayer::Update(const FAnimationUpdateContext& Context)
{
	if(!Sequence)
	{
		return;
	}

	CurrentPlayTime += Context.GetDeltaTime() * PlayRate;

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
