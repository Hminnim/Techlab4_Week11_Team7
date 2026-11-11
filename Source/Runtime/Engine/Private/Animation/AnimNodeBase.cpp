#include "EnginePCH.h"
#include "Engine/Classes/Animation/AnimNodeBase.h"

FAnimationBaseContext::FAnimationBaseContext() = default;

FAnimationBaseContext::FAnimationBaseContext(UAnimInstance* InAnimInstance)
	: AnimInstance(InAnimInstance)
{
}

FAnimationBaseContext::FAnimationBaseContext(FAnimInstanceProxy* InAnimInstanceProxy, FAnimationUpdateSharedContext* InSharedContext)
	: AnimInstanceProxy(InAnimInstanceProxy)
	, SharedContext(InSharedContext)
{
}

void FAnimNode_Base::Initialize(const FAnimationInitializeContext&)
{
}

void FAnimNode_Base::Update(const FAnimationUpdateContext&)
{
}

void FAnimNode_Base::Evaluate(FPoseContext& Output)
{
	Output.Pose.ResetToRefPose();
}
