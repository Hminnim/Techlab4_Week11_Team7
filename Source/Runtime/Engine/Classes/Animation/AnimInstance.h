#pragma once

#include "UObject/Object.h"
#include "Engine/Classes/Animation/AnimTypes.h"
#include "AnimInstance.generated.h"

struct FAnimationUpdateContext;
struct FPoseContext;
class USkeletalMeshComponent;
class APawn;

struct FAnimNode_Base
{
	virtual ~FAnimNode_Base() = default;
	virtual void Update(const FAnimationUpdateContext& Context) {}
	virtual void Evaluate(FPoseContext& Output) {}
};

UCLASS()
class UAnimInstance : public UObject
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() {}              // 그래프를 만든다 (한 번)
	virtual void NativeUpdateAnimation(float DeltaSeconds) {} // 변수를 갱신한다 (매 프레임)

	void TriggerAnimNotifies(float DeltaSeconds) {}

	USkeletalMeshComponent* GetSkelMeshComponent() const { return OwningComponent; }
	APawn* TryGetPawnOwner() const { return nullptr; }

protected:
	USkeletalMeshComponent* OwningComponent = nullptr;
	FAnimNode_Base* RootNode = nullptr; // 그래프의 시작 노드 (StateMachine 등)
};

UCLASS()
class UAnimSingleNodeInstance : public UAnimInstance
{
	GENERATED_BODY()
};
