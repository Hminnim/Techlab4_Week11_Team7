#include "EnginePCH.h"
#include "AnimNode_BlendSpacePlayer.h"
#include "BlendSpace.h"
#include "AnimInstance.h"

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
        const float PrevNormTime = InternalTimeAccumulator;
        const float DeltaNorm = (Context.GetDeltaTime() * PlayRate) / Length;

        InternalTimeAccumulator += DeltaNorm;
        if(bLoop)
        {
            while(InternalTimeAccumulator >= 1.0f) InternalTimeAccumulator -= 1.0f;
            while(InternalTimeAccumulator < 0.0f)  InternalTimeAccumulator += 1.0f;
        }
        else
        {
            InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.0f, 1.0f);
        }
        
        float MaxTotalWeight = 0.0f;
        FBlendSampleData* Leader = nullptr;

        for(FBlendSampleData& Data : BlendSampleDataCache)
        { 
            const FBlendSample& Sample = BlendSpace->GetBlendSample(Data.SampleDataIndex);
            Data.Time = InternalTimeAccumulator * Sample.GetSamplePlayLength();

            if(Data.Animation && Data.TotalWeight > MaxTotalWeight)
            {
                MaxTotalWeight = Data.TotalWeight;
                Leader = &Data;
            }
        }

        FAnimNotifyQueue* NotifyQueue = Context.GetAnimInstance() ? &Context.GetAnimInstance()->NotifyQueue : nullptr;
        if(NotifyQueue && MaxTotalWeight > 0 && Leader != nullptr)
        {
            const float SampleLength = Leader->Animation->GetPlayLength();
            const float LeaderPrevTime = PrevNormTime * SampleLength;
            const float LeaderDeltaMove = DeltaNorm * SampleLength;

            FAnimNotifyContext NotifyContext;
            NotifyContext.bLooping = bLoop;
            Leader->Animation->GetAnimNotifies(LeaderPrevTime, LeaderDeltaMove, NotifyContext);            
            NotifyQueue->AddAnimNotifies(NotifyContext.ActiveNotifies, NotifyQueue->AnimNotifies);
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
