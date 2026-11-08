#pragma once

#include "AnimNodeBase.h"
#include "AnimSequence.h"

struct FAnimNode_SequencePlayer :public FAnimNode_Base
{
	UAnimSequence* Sequence = nullptr;

	float PlayRateBasis = 1.0f;
	float PlayRate = 1.0f;
	float CurrentPlayTime = 0.0f;
	bool bLoopAnimation = false;

	// Interfaces
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;

	UAnimSequence* GetSequence() const { return Sequence; }
	void SetSequence(UAnimSequence* NewSequence) { Sequence = NewSequence; }
	
	float GetPlayRateBasis() const { return PlayRateBasis; }
	void SetPlayRateBasis(float NewPlayRateBasis) { PlayRateBasis = NewPlayRateBasis; }

	float GetPlayRate() const { return PlayRate; }
	void SetPlayRate(float NewPlayRate) { PlayRate = NewPlayRate; }

	float GetCurrentPlayTime() const { return CurrentPlayTime; }
	void SetCurrentPlayTime(float NewCurrentPlayTime) { CurrentPlayTime = NewCurrentPlayTime; }

	bool IsLoopAnimation() const { return bLoopAnimation; }
	void SetLoopAnimation(bool NewLoopAnimation) { bLoopAnimation = NewLoopAnimation; }
};