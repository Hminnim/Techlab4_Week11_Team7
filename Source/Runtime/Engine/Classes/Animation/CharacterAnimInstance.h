#pragma once

#include "AnimInstance.h"
#include "AnimNode_BlendSpacePlayer.h"
#include "AnimNode_StateMachine.h"
#include "CharacterAnimInstance.generated.h"

class UAnimSequence;

struct FCharacterJumpClips
{
	UAnimSequence* Start = nullptr;
	UAnimSequence* Loop = nullptr;
	UAnimSequence* Land = nullptr;
};

// 캐릭터별 입력과 애니메이션 그래프의 실행 상태를 보관한다.
UCLASS()
class UCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// BlendSpace가 없으면 Idle을 반복한다. Attack과 Hit은 한 번 재생한다.
	bool ConfigureAnimations(UAnimSequence* Idle, UAnimSequence* Attack, UAnimSequence* Hit,
		const FCharacterJumpClips& Jump = {}, TUniquePtr<FAnimNode_BlendSpacePlayer> BlendSpace = nullptr);
	bool ConfigureGraph(FAnimNode_StateMachine&& Graph);

	// 초기화 전에 한 번 호출한다. 노드는 이 인스턴스가 소유하며, 완료 조건은 실제 재생 노드에서 읽는다.
	bool ConfigureStateMachine(TUniquePtr<FAnimNode_Base> LocomotionNode,
		TUniquePtr<FAnimNode_Base> AttackNode, TUniquePtr<FAnimNode_Base> HitNode,
		std::function<bool()> IsAttackFinished, std::function<bool()> IsHitFinished,
		const FCharacterJumpClips& Jump = {});

	// 호출자는 NativeInitialize 후 루트 Initialize, NativeUpdate 후 루트 Update/Evaluate를 실행한다.
	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;
	void HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName) override;
	void HandleAnimNotifyBegin(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName, float TotalDuration) override;
	void HandleAnimNotifyEnd(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName) override;

	void SetSpeed(float InSpeed) { Speed = InSpeed; }
	float GetSpeed() const { return Speed; }
	FAnimNode_BlendSpacePlayer* GetLocomotionBlendSpace() const { return LocomotionBlendSpace; }
	void RequestAttack() { bPendingAttack = true; }
	void SetAttackTransitionRule(std::function<bool(bool)> Rule);
	void RequestHit() { bPendingHit = true; }
	bool RequestJump();
	void NotifyLanded();
	void SetRestartHitOnReentry(bool bEnabled);
	bool GetRestartHitOnReentry() const { return bRestartHitOnReentry; }
	FName GetCurrentStateName() const { return StateMachine.GetCurrentStateName(); }
	const FAnimStateMachine& GetStateMachine() const { return StateMachine.GetStateMachine(); }

private:
	enum class EJumpPhase
	{
		Grounded,
		Airborne,
		Landed
	};

	FAnimNode_StateMachine StateMachine;
	FAnimNode_BlendSpacePlayer* LocomotionBlendSpace = nullptr;
	std::function<bool(bool)> AttackTransitionRule;
	int32 HitState = -1;
	int32 JumpFromState = -1;
	EJumpPhase JumpPhase = EJumpPhase::Grounded;
	float Speed = 0.0f;
	bool bPendingAttack = false;
	bool bPendingHit = false;
	bool bAttackRequested = false;
	bool bHitRequested = false;
	bool bRestartHitOnReentry = false;
};
