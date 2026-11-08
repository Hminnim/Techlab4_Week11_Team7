#include "EnginePCH.h"
#include "AnimNode_TwoWayBlend.h"
#include "AnimationRuntime.h"

void FAnimNode_TwoWayBlend::Initialize(const FAnimationInitializeContext& Context)
{
	A.Initialize(Context);
	B.Initialize(Context);
}

void FAnimNode_TwoWayBlend::Update(const FAnimationUpdateContext& Context)
{
	A.Update(Context.FractionalWeight(1.0f - Alpha));
	B.Update(Context.FractionalWeight(Alpha));
}

void FAnimNode_TwoWayBlend::Evaluate(FPoseContext& Output)
{
	FPoseContext Pose1(Output);
	FPoseContext Pose2(Output);

	A.Evaluate(Pose1);
	B.Evaluate(Pose2);

	FAnimationRuntime::BlendTwoPosesTogether(Pose1.Pose, Pose2.Pose, Pose1.Curve, Pose2.Curve, 1.0f - Alpha, Output.Pose, Output.Curve);
}
