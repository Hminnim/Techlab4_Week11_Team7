#include "EnginePCH.h"
#include "AnimNode_BlendSpacePlayer.h"
#include "BlendSpace.h"

void FAnimNode_BlendSpacePlayer::Initialize(const FAnimationInitializeContext& Context)
{
	InternalTimeAccumulator = 0.0f;
	BlendSampleDataCache.Reset();
}

void FAnimNode_BlendSpacePlayer::Update(const FAnimationUpdateContext& Context)
{
    if(!BlendSpace) return;
    
    BlendSpace->GetSamplesFromBlendInput(FVector(X, Y, 0.0f), BlendSampleDataCache);
    
    float Length = BlendSpace->GetAnimationLengthFromSampleData(BlendSampleDataCache);
    if(Length > 0.0f)
    {
        InternalTimeAccumulator += (Context.GetDeltaTime() * PlayRate) / Length;
        if(bLoop)
        {
            while(InternalTimeAccumulator >= 1.0f) InternalTimeAccumulator -= 1.0f;
            while(InternalTimeAccumulator < 0.0f)  InternalTimeAccumulator += 1.0f;
        }
        else
        {
            InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.0f, 1.0f);
        }
        
        for(FBlendSampleData& Data : BlendSampleDataCache)
        { 
            const FBlendSample& Sample = BlendSpace->GetBlendSample(Data.SampleDataIndex);
            Data.Time = InternalTimeAccumulator * Sample.GetSamplePlayLength();
        }
    }
}

void FAnimNode_BlendSpacePlayer::Evaluate(FPoseContext& Output)
{
    if(!BlendSpace)
    {
        Output.Pose.ResetToRefPose();
        return;
    }
    
    BlendSpace->GetAnimationPose(BlendSampleDataCache, Output.Pose, Output.Curve);
}
