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

	// 에셋의 RateScale까지 곱한 실제 재생 속도. 음수면 역재생.
	float GetEffectivePlayRate() const { return Sequence ? PlayRate * Sequence->RateScale : PlayRate; }

	// 재생 방향으로 잰 경과 시간. 역재생이면 끝에서부터 재므로, 길이에 닿으면 어느 방향이든 재생이 끝난 것.
	float GetCurrentAssetTimePlayRateAdjusted() const
	{
		if (!Sequence)
			return CurrentPlayTime;
		return GetEffectivePlayRate() < 0.0f ? Sequence->GetPlayLength() - CurrentPlayTime : CurrentPlayTime;
	}
};