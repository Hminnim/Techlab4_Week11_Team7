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
using FAnimNotifyStateBeginDelegate = TDelegate<const FName&, const FString&, float>;
using FAnimNotifyStateTickDelegate = TDelegate<const FName&, const FString&, float>;
using FAnimNotifyStateEndDelegate = TDelegate<const FName&, const FString&>;

struct FAnimNotifyStateHandlers
{
	FAnimNotifyStateBeginDelegate OnBegin;
	FAnimNotifyStateTickDelegate OnTick;
	FAnimNotifyStateEndDelegate OnEnd;
};

struct FActiveAnimNotifyState
{
	// 매칭 전용 포인터(수정/재할당 시 역참조하지 않음)
	const FAnimNotifyEvent* NotifyPtr = nullptr;
	const UAnimSequenceBase* NotifySource = nullptr;
	FAnimNotifyEvent Event;
	FString ClipName;
	float ElapsedTime = 0.0f;
	bool bTouchedThisFrame = false;
};

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
	FAnimNotifyStateBeginDelegate OnAnimNotifyBegin;
	FAnimNotifyStateTickDelegate OnAnimNotifyTick;
	FAnimNotifyStateEndDelegate OnAnimNotifyEnd;

	void TriggerAnimNotifies(float DeltaSeconds);
	void EndActiveNotifyStates();
	bool IsNotifyStateActive(const FName& NotifyName) const;
	const TArray<FActiveAnimNotifyState>& GetActiveNotifyStates() const { return ActiveNotifyStates; }

	virtual void HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName) {}
	virtual void HandleAnimNotifyBegin(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName, float TotalDuration) {}
	virtual void HandleAnimNotifyTick(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName, float FrameDeltaTime) {}
	virtual void HandleAnimNotifyEnd(const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* NotifySource, const FString& ClipName) {}

	TMap<FName, TUniquePtr<FAnimNotifyDelegate>, FNameHash> ExternalNotifyHandlers;
	TMap<FName, TUniquePtr<FAnimNotifyStateHandlers>, FNameHash> ExternalNotifyStateHandlers;

	FDelegateHandle AddExternalNotifyHandler(const FName& NotifyName, std::function<void()> Handler);

	template<typename T>
	FDelegateHandle AddExternalNotifyHandler(const FName& NotifyName, T* Instance, void (T::* Func)());

	FAnimNotifyDelegate& GetOrAddNotifyDelegate(const FName& NotifyName);
	FAnimNotifyStateHandlers& GetOrAddNotifyStateHandlers(const FName& NotifyName);

private:
	void DispatchSingleAnimNotify(const FAnimNotifyEvent& Event, const UAnimSequenceBase* Source, const FString& ClipName);
	void DispatchNotifyStateBegin(const FAnimNotifyEvent& Event, const UAnimSequenceBase* Source, const FString& ClipName);
	void DispatchNotifyStateTick(const FAnimNotifyEvent& Event, const UAnimSequenceBase* Source, const FString& ClipName, float FrameDeltaTime);
	void DispatchNotifyStateEnd(const FAnimNotifyEvent& Event, const UAnimSequenceBase* Source, const FString& ClipName);

	TArray<FActiveAnimNotifyState> ActiveNotifyStates;
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
