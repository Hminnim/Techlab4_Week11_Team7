#pragma once

#include "Engine/Classes/Animation/AnimInstance.h"
#include "Engine/Classes/Animation/AnimNode_BlendSpacePlayer.h"
#include "Engine/Classes/Animation/AnimNode_SequencePlayer.h"
#include "Engine/Classes/Animation/AnimNode_TwoWayBlend.h"

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

// Two-Way Blend 테스트 및 시각화용 애니메이션 인스턴스
class UTwoWayBlendAnimInstance : public UAnimInstance
{
public:
	FAnimNode_SequencePlayer PlayerA;
	FAnimNode_BlendSpacePlayer BlendSpaceA;
	FAnimNode_SequencePlayer PlayerB;
	FAnimNode_TwoWayBlend TwoWayBlend;
	float Alpha = 0.0f;
	float BlendSpaceSpeed = 0.0f;
	bool bUseBlendSpaceForA = false;

	UTwoWayBlendAnimInstance()
	{
		TwoWayBlend.A.SetLinkNode(&PlayerA);
		TwoWayBlend.B.SetLinkNode(&PlayerB);
		RootNode = &TwoWayBlend;
	}

	void SetClips(UAnimSequence* ClipA, UAnimSequence* ClipB, bool bLoopA = true, bool bLoopB = true)
	{
		PlayerA.SetSequence(ClipA);
		PlayerA.SetLoopAnimation(bLoopA);
		PlayerB.SetSequence(ClipB);
		PlayerB.SetLoopAnimation(bLoopB);
		RootNode = &TwoWayBlend;
	}

	void SetBlendSpaceForA(UBlendSpace* InBlendSpace)
	{
		BlendSpaceA.BlendSpace.reset(InBlendSpace);
	}

	void SetUseBlendSpaceForA(bool bEnable)
	{
		bUseBlendSpaceForA = bEnable && (BlendSpaceA.BlendSpace != nullptr);
		TwoWayBlend.A.SetLinkNode(bUseBlendSpaceForA
			? static_cast<FAnimNode_Base*>(&BlendSpaceA)
			: static_cast<FAnimNode_Base*>(&PlayerA));
		RootNode = &TwoWayBlend;
	}

	void NativeUpdateAnimation(float DeltaSeconds) override
	{
		(void)DeltaSeconds;
		BlendSpaceA.X = BlendSpaceSpeed;
		TwoWayBlend.Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	}
};
