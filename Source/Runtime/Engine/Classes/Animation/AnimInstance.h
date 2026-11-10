#pragma once

#include "UObject/Object.h"
#include "Engine/Classes/Animation/AnimTypes.h"
#include "AnimInstance.generated.h"

struct FAnimationUpdateContext;
struct FPoseContext;
class USkeletalMeshComponent;
class APawn;
struct FAnimNode_Base;

UCLASS()
class UAnimInstance : public UObject
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() {}              // 그래프를 만든다 (한 번)
	virtual void NativeUpdateAnimation(float DeltaSeconds) {} // 변수를 갱신한다 (매 프레임)

	void TriggerAnimNotifies(float DeltaSeconds) {}
	void AccumulateRootMotion(const FTransform& Delta, float Weight) { RootMotion.AccumulateWithBlend(Delta, Weight); }
	FRootMotionMovementParams ConsumeRootMotion()
	{
		FRootMotionMovementParams Consume = RootMotion;
		RootMotion.Clear();
		return Consume;
	}

	USkeletalMeshComponent* GetSkelMeshComponent() const { return OwningComponent; }
	void SetOwningComponent(USkeletalMeshComponent* InComponent) { OwningComponent = InComponent; }
	FAnimNode_Base* GetRootNode() const { return RootNode; }
	APawn* TryGetPawnOwner() const { return nullptr; }

protected:
	USkeletalMeshComponent* OwningComponent = nullptr;
	FAnimNode_Base* RootNode = nullptr; // 그래프의 시작 노드 (StateMachine 등)

private:
	FRootMotionMovementParams RootMotion;
};

UCLASS()
class UAnimSingleNodeInstance : public UAnimInstance
{
	GENERATED_BODY()
};
