#pragma once

#include "UObject/Object.h"
#include "Engine/Classes/Animation/AnimTypes.h"
#include "Core/Delegate/Delegate.h"
#include "Containers/Map.h"
#include "AnimNotifyQueue.h"
#include "AnimInstance.generated.h"

struct FAnimationUpdateContext;
struct FPoseContext;
class USkeletalMeshComponent;
class APawn;
struct FAnimNode_Base;

using FAnimNotifyDelegate = TDelegate<>;
using FAnimNotifyEventDelegate = TDelegate<const FName&, const FString&>;

UCLASS()
class UAnimInstance : public UObject
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() {}              // 그래프를 만든다 (한 번)
	virtual void NativeUpdateAnimation(float DeltaSeconds) {} // 변수를 갱신한다 (매 프레임)	
	
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

public:
	FAnimNotifyQueue NotifyQueue;
	FAnimNotifyEventDelegate OnAnimNotify;

	void TriggerAnimNotifies(float DeltaSeconds);
	virtual void HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName) {}

	TMap<FName, TUniquePtr<FAnimNotifyDelegate>, FNameHash> ExternalNotifyHandlers;

	FDelegateHandle AddExternalNotifyHandler(const FName& NotifyName, std::function<void()> Handler);

	template<typename T>
	FDelegateHandle AddExternalNotifyHandler(const FName& NotifyName, T* Instance, void (T::* Func)());

	FAnimNotifyDelegate& GetOrAddNotifyDelegate(const FName& NotifyName);

private:
	FRootMotionMovementParams RootMotion;
};

UCLASS()
class UAnimSingleNodeInstance : public UAnimInstance
{
	GENERATED_BODY();
};

template<typename T>
inline FDelegateHandle UAnimInstance::AddExternalNotifyHandler(const FName& NotifyName, T* Instance, void(T::* Func)())
{
	FAnimNotifyDelegate& Delegate = GetOrAddNotifyDelegate(NotifyName);
	if constexpr(std::is_base_of_v<UObject, T>)
	{
		return Delegate.AddDynamic(Instance, Func);
	}
	else
	{
		return Delegate.AddRaw(Instance, Func);
	}
}
