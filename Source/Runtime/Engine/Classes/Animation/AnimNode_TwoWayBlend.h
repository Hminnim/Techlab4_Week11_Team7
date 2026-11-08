#pragma once

#include "AnimNodeBase.h"

struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	FPoseLink A;
	FPoseLink B;

	float Alpha = 0.0f;

	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
};