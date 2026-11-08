#pragma once

#include "AnimInstance.h"
#include "AnimNode_StateMachine.h"
#include "CharacterAnimInstance.generated.h"

class UAnimSequence;

// 캐릭터별 입력과 Locomotion / Attack / Hit 그래프의 실행 상태를 보관한다.
UCLASS()
class UCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 이미 로드된 클립으로 구성한다. Idle은 반복, Attack과 Hit은 한 번 재생한다.
	bool ConfigureAnimations(UAnimSequence* Idle, UAnimSequence* Attack, UAnimSequence* Hit);

	// 초기화 전에 한 번 호출한다. 노드는 이 인스턴스가 소유하며, 완료 조건은 실제 재생 노드에서 읽는다.
	bool ConfigureStateMachine(TUniquePtr<FAnimNode_Base> LocomotionNode,
		TUniquePtr<FAnimNode_Base> AttackNode, TUniquePtr<FAnimNode_Base> HitNode,
		std::function<bool()> IsAttackFinished, std::function<bool()> IsHitFinished);

	// 호출자는 NativeInitialize 후 루트 Initialize, NativeUpdate 후 루트 Update/Evaluate를 실행한다.
	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;

	void SetSpeed(float InSpeed) { Speed = InSpeed; }
	float GetSpeed() const { return Speed; }
	void RequestAttack() { bPendingAttack = true; }
	void RequestHit() { bPendingHit = true; }
	void SetRestartHitOnReentry(bool bEnabled);
	bool GetRestartHitOnReentry() const { return bRestartHitOnReentry; }
	FName GetCurrentStateName() const { return StateMachine.GetCurrentStateName(); }

private:
	FAnimNode_StateMachine StateMachine;
	int32 HitState = -1;
	float Speed = 0.0f;
	bool bPendingAttack = false;
	bool bPendingHit = false;
	bool bAttackRequested = false;
	bool bHitRequested = false;
	bool bRestartHitOnReentry = false;
};
