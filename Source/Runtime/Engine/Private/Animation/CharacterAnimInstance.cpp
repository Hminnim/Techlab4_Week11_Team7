#include "EnginePCH.h"
#include "Engine/Classes/Animation/CharacterAnimInstance.h"
#include "Engine/Classes/Animation/AnimNode_SequencePlayer.h"

#include <utility>

namespace
{
	bool IsPlayableClip(const UAnimSequence* Sequence)
	{
		return Sequence && Sequence->GetPlayLength() > 0.0f;
	}

	TUniquePtr<FAnimNode_SequencePlayer> MakeSequencePlayer(UAnimSequence* Sequence, bool bLoop)
	{
		auto Player = MakeUnique<FAnimNode_SequencePlayer>();
		Player->SetSequence(Sequence);
		Player->SetLoopAnimation(bLoop);
		return Player;
	}

	auto MakeClipFinishedRule(const FAnimNode_SequencePlayer* Player)
	{
		// 노드 소유권은 상태 머신으로 이동하지만 주소는 유지된다.
		return [Player]()
		{
				return !Player->IsLoopAnimation() 
					&& Player->GetCurrentAssetTimePlayRateAdjusted() >= Player->GetSequence()->GetPlayLength();
		};
	}
}

bool UCharacterAnimInstance::ConfigureAnimations(UAnimSequence* Idle, UAnimSequence* Attack, UAnimSequence* Hit,
	const FCharacterJumpClips& Jump, TUniquePtr<FAnimNode_BlendSpacePlayer> BlendSpace)
{
	if (RootNode || !IsPlayableClip(Idle) || !IsPlayableClip(Attack) || !IsPlayableClip(Hit))
	{
		return false;
	}

	// AnimNotify 테스트: 클립에 테스트 이벤트 등록 (공유 에셋 중복 방지)
	if (Attack->Notifies.IsEmpty())
	{
		FAnimNotifyEvent AttackEvent;
		AttackEvent.TriggerTime = Attack->GetPlayLength() * 0.4f;
		AttackEvent.NotifyName = FName("Attack_Slash");
		Attack->Notifies.Add(AttackEvent);
	}
	if (Hit->Notifies.IsEmpty())
	{
		FAnimNotifyEvent HitEvent;
		HitEvent.TriggerTime = Hit->GetPlayLength() * 0.2f;
		HitEvent.NotifyName = FName("Hit_Impact");
		Hit->Notifies.Add(HitEvent);
	}
	if (Jump.Land && Jump.Land->Notifies.IsEmpty())
	{
		FAnimNotifyEvent LandEvent;
		LandEvent.TriggerTime = 0.05f;
		LandEvent.NotifyName = FName("Jump_Landing");
		Jump.Land->Notifies.Add(LandEvent);
	}

	auto AttackPlayer = MakeSequencePlayer(Attack, false);
	auto HitPlayer = MakeSequencePlayer(Hit, false);
	const auto IsAttackFinished = MakeClipFinishedRule(AttackPlayer.get());
	const auto IsHitFinished = MakeClipFinishedRule(HitPlayer.get());
	auto* BlendSpacePlayer = BlendSpace.get();
	TUniquePtr<FAnimNode_Base> LocomotionPlayer = std::move(BlendSpace);
	if (!LocomotionPlayer) LocomotionPlayer = MakeSequencePlayer(Idle, true);
	if (!ConfigureStateMachine(std::move(LocomotionPlayer), std::move(AttackPlayer), std::move(HitPlayer),
		IsAttackFinished, IsHitFinished, Jump)) return false;
	LocomotionBlendSpace = BlendSpacePlayer;
	return true;
}

bool UCharacterAnimInstance::ConfigureGraph(FAnimNode_StateMachine&& Graph)
{
	if (RootNode || Graph.GetCurrentState() < 0) return false;
	StateMachine = std::move(Graph);
	RootNode = &StateMachine;
	return true;
}

bool UCharacterAnimInstance::ConfigureStateMachine(TUniquePtr<FAnimNode_Base> LocomotionNode,
	TUniquePtr<FAnimNode_Base> AttackNode, TUniquePtr<FAnimNode_Base> HitNode,
	std::function<bool()> IsAttackFinished, std::function<bool()> IsHitFinished,
	const FCharacterJumpClips& Jump)
{
	if (RootNode || !LocomotionNode || !AttackNode || !HitNode || !IsAttackFinished || !IsHitFinished)
	{
		return false;
	}
	const bool bHasJump = Jump.Start || Jump.Loop || Jump.Land;
	if (bHasJump && (!IsPlayableClip(Jump.Start) || !IsPlayableClip(Jump.Loop) || !IsPlayableClip(Jump.Land)))
	{
		return false;
	}

	const int32 LocomotionState = StateMachine.AddState(FName("Locomotion"), std::move(LocomotionNode));
	const int32 AttackState = StateMachine.AddState(FName("Attack"), std::move(AttackNode));
	HitState = StateMachine.AddState(FName("Hit"), std::move(HitNode));

	StateMachine.AddAnyStateTransition(HitState, [this]() { return bHitRequested; });
	if (bHasJump)
	{
		JumpFromState = LocomotionState;
		auto StartPlayer = MakeSequencePlayer(Jump.Start, false);
		auto LandPlayer = MakeSequencePlayer(Jump.Land, false);
		const auto IsStartFinished = MakeClipFinishedRule(StartPlayer.get());
		const auto IsLandFinished = MakeClipFinishedRule(LandPlayer.get());
		const int32 JumpStartState = StateMachine.AddState(FName("JumpStart"), std::move(StartPlayer));
		const int32 JumpLoopState = StateMachine.AddState(FName("JumpLoop"), MakeSequencePlayer(Jump.Loop, true));
		const int32 JumpLandState = StateMachine.AddState(FName("JumpLand"), std::move(LandPlayer));

		const auto ConsumeLanding = [this]()
		{
			if (JumpPhase != EJumpPhase::Landed) return false;
			JumpPhase = EJumpPhase::Grounded;
			return true;
		};
		StateMachine.AddTransition(LocomotionState, JumpStartState, [this]() { return JumpPhase != EJumpPhase::Grounded; });
		StateMachine.AddTransition(JumpStartState, JumpLandState, ConsumeLanding);
		StateMachine.AddTransition(JumpStartState, JumpLoopState, IsStartFinished);
		StateMachine.AddTransition(JumpLoopState, JumpLandState, ConsumeLanding);
		StateMachine.AddTransition(JumpLandState, LocomotionState, IsLandFinished);
		StateMachine.AddTransition(HitState, JumpLoopState, [this, IsHitFinished]()
		{
			return JumpPhase == EJumpPhase::Airborne && IsHitFinished();
		});
		StateMachine.AddTransition(HitState, JumpLandState, [this, IsHitFinished, ConsumeLanding]()
		{
			return JumpPhase == EJumpPhase::Landed && IsHitFinished() && ConsumeLanding();
		});
	}
	StateMachine.AddTransition(LocomotionState, AttackState, [this]()
	{
		return AttackTransitionRule ? AttackTransitionRule(bAttackRequested) : bAttackRequested;
	});
	StateMachine.AddTransition(AttackState, LocomotionState, std::move(IsAttackFinished));
	StateMachine.AddTransition(HitState, LocomotionState, [this, IsHitFinished = std::move(IsHitFinished)]()
	{
		return JumpPhase == EJumpPhase::Grounded && IsHitFinished();
	});
	StateMachine.SetStateRestartOnReentry(HitState, bRestartHitOnReentry);
	StateMachine.Start(LocomotionState);
	RootNode = &StateMachine;
	return true;
}

void UCharacterAnimInstance::NativeInitializeAnimation()
{
	Speed = 0.0f;
	JumpPhase = EJumpPhase::Grounded;
	bPendingAttack = false;
	bPendingHit = false;
	bAttackRequested = false;
	bHitRequested = false;
}

void UCharacterAnimInstance::SetAttackTransitionRule(std::function<bool(bool)> Rule)
{
	AttackTransitionRule = std::move(Rule);
}

bool UCharacterAnimInstance::RequestJump()
{
	if (JumpFromState < 0 || StateMachine.GetCurrentState() != JumpFromState || JumpPhase != EJumpPhase::Grounded)
	{
		return false;
	}
	JumpPhase = EJumpPhase::Airborne;
	return true;
}

void UCharacterAnimInstance::NotifyLanded()
{
	if (JumpPhase == EJumpPhase::Airborne)
	{
		JumpPhase = EJumpPhase::Landed;
	}
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	if (LocomotionBlendSpace) LocomotionBlendSpace->X = Speed;

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

void UCharacterAnimInstance::HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName)
{
	(void)NotifySource;
	HTR_LOG(Info, "[CharacterAnimInstance] Received Notify '{}' from '{}' (State: {})",
		NotifyEvent.NotifyName.ToString(), ClipName, GetCurrentStateName().ToString());
}

void UCharacterAnimInstance::HandleAnimNotifyBegin(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName, float TotalDuration)
{
	(void)NotifySource;
	HTR_LOG(Info, "[CharacterAnimInstance] NotifyState Begin '{}' from '{}' (Duration: {:.3f}s, State: {})",
		NotifyEvent.NotifyName.ToString(), ClipName, TotalDuration, GetCurrentStateName().ToString());
}

void UCharacterAnimInstance::HandleAnimNotifyEnd(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName)
{
	(void)NotifySource;
	HTR_LOG(Info, "[CharacterAnimInstance] NotifyState End '{}' from '{}' (State: {})",
		NotifyEvent.NotifyName.ToString(), ClipName, GetCurrentStateName().ToString());
}
