#pragma once

#include "Engine/Classes/Animation/AnimInstance.h"
#include "Engine/Classes/Animation/AnimNode_BlendSpacePlayer.h"

// 1D 블렌드스페이스 테스트 및 시각화용 애니메이션 인스턴스
class UBlendSpaceAnimInstance : public UAnimInstance
{
public:
	FAnimNode_BlendSpacePlayer BlendSpacePlayer;
	float Speed = 0.0f;

	void SetBlendSpace(UBlendSpace* InBlendSpace)
	{
		BlendSpacePlayer.BlendSpace.reset(InBlendSpace);
		RootNode = &BlendSpacePlayer;
	}

	void NativeUpdateAnimation(float DeltaSeconds) override
	{
		BlendSpacePlayer.X = Speed;
	}
};
