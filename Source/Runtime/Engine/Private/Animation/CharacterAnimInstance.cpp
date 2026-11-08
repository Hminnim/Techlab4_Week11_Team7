#include "EnginePCH.h"
#include "Engine/Classes/Animation/CharacterAnimInstance.h"
#include "Engine/Classes/Animation/AnimNode_SequencePlayer.h"

#include <utility>

bool UCharacterAnimInstance::ConfigureAnimations(UAnimSequence* Idle, UAnimSequence* Attack, UAnimSequence* Hit)
{
	if (RootNode || !Idle || !Attack || !Hit)
	{
		return false;
	}
	if (Idle->GetPlayLength() <= 0.0f || Attack->GetPlayLength() <= 0.0f || Hit->GetPlayLength() <= 0.0f)
	{
		return false;
	}

	auto IdlePlayer = MakeUnique<FAnimNode_SequencePlayer>();
	IdlePlayer->SetSequence(Idle);
	IdlePlayer->SetLoopAnimation(true);

	auto AttackPlayer = MakeUnique<FAnimNode_SequencePlayer>();
	AttackPlayer->SetSequence(Attack);
	AttackPlayer->SetLoopAnimation(false);

	auto HitPlayer = MakeUnique<FAnimNode_SequencePlayer>();
	HitPlayer->SetSequence(Hit);
	HitPlayer->SetLoopAnimation(false);

	// 소유권을 넘겨도 노드 주소는 유지된다. 종료 조건은 실제 재생 시간을 읽는다.
	FAnimNode_SequencePlayer* AttackNode = AttackPlayer.get();
	FAnimNode_SequencePlayer* HitNode = HitPlayer.get();
	return ConfigureStateMachine(std::move(IdlePlayer), std::move(AttackPlayer), std::move(HitPlayer),
		[AttackNode]()
		{
			return AttackNode->GetCurrentPlayTime() >= AttackNode->GetSequence()->GetPlayLength();
		},
		[HitNode]()
		{
			return HitNode->GetCurrentPlayTime() >= HitNode->GetSequence()->GetPlayLength();
		});
}

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
