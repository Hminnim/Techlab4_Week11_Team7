#pragma once

#include "AnimNodeBase.h"
#include "BlendSpace.h"

struct FAnimNode_BlendSpacePlayer : public FAnimNode_Base
{
	TUniquePtr<UBlendSpace> BlendSpace;
	TArray<FBlendSampleData> BlendSampleDataCache;

	float X = 0.0f;
	float Y = 0.0f;
	float PlayRate = 1.0f;
	bool bLoop = true;

	float InternalTimeAccumulator = 0.0f;

	// interfaces
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
};
