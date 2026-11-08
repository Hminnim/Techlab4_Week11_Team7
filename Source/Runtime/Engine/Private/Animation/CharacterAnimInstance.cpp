#include "EnginePCH.h"
#include "Engine/Classes/Animation/CharacterAnimInstance.h"

#include <utility>

bool UCharacterAnimInstance::ConfigureStateMachine(TUniquePtr<FAnimNode_Base> LocomotionNode,
	TUniquePtr<FAnimNode_Base> AttackNode, TUniquePtr<FAnimNode_Base> HitNode,
	std::function<bool()> IsAttackFinished, std::function<bool()> IsHitFinished)
{
	if (RootNode || !LocomotionNode || !AttackNode || !HitNode || !IsAttackFinished || !IsHitFinished)
	{
		return false;
	}

	const int32 LocomotionState = StateMachine.AddState(FName("Locomotion"), std::move(LocomotionNode));
	const int32 AttackState = StateMachine.AddState(FName("Attack"), std::move(AttackNode));
	HitState = StateMachine.AddState(FName("Hit"), std::move(HitNode));

	StateMachine.AddAnyStateTransition(HitState, [this]() { return bHitRequested; });
	StateMachine.AddTransition(LocomotionState, AttackState, [this]() { return bAttackRequested; });
	StateMachine.AddTransition(AttackState, LocomotionState, std::move(IsAttackFinished));
	StateMachine.AddTransition(HitState, LocomotionState, std::move(IsHitFinished));
	StateMachine.SetStateRestartOnReentry(HitState, bRestartHitOnReentry);
	StateMachine.Start(LocomotionState);
	RootNode = &StateMachine;
	return true;
}

void UCharacterAnimInstance::NativeInitializeAnimation()
{
	Speed = 0.0f;
	bPendingAttack = false;
	bPendingHit = false;
	bAttackRequested = false;
	bHitRequested = false;
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	// 이 호출 다음에 RootNode->Update가 실행된다. 입력은 그 프레임에만 유효하다.
	bAttackRequested = bPendingAttack;
	bHitRequested = bPendingHit;
	bPendingAttack = false;
	bPendingHit = false;
}

void UCharacterAnimInstance::SetRestartHitOnReentry(bool bEnabled)
{
	bRestartHitOnReentry = bEnabled;
	if (HitState >= 0)
	{
		StateMachine.SetStateRestartOnReentry(HitState, bEnabled);
	}
}
