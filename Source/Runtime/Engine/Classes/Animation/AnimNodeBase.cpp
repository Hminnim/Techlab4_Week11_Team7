#include "EnginePCH.h"
#include "AnimNodeBase.h"

FAnimationBaseContext::FAnimationBaseContext(UAnimInstance* InAnimInstance)
{
	AnimInstance = InAnimInstance;
}

void FPoseLink::Initialize(const FAnimationInitializeContext& Context)
{
	if(LinkedNode)
	{
		LinkedNode->Initialize(Context);
	}
}

void FPoseLink::Update(const FAnimationUpdateContext& Context)
{
	if(LinkedNode)
	{
		LinkedNode->Update(Context);
	}
}

void FPoseLink::Evaluate(FPoseContext& Output)
{
	if(LinkedNode)
	{
		LinkedNode->Evaluate(Output);
	}
}

void FPoseLink::SetLinkNode(FAnimNode_Base* NewLinkNode)
{
	LinkedNode = NewLinkNode;
}
