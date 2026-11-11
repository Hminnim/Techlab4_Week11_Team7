#pragma once

#include "AnimNodeBase.h"
#include "AnimStateMachine.h"

// 전이 판단 코어와 상태별 포즈 노드를 연결한다. 현재는 전환 즉시 포즈를 교체한다.
struct FAnimNode_StateMachine : public FAnimNode_Base
{
public:
	int32 AddState(FName Name, TUniquePtr<FAnimNode_Base> Node);
	bool AddTransition(int32 FromState, int32 ToState, std::function<bool()> Rule);
	bool AddAnyStateTransition(int32 ToState, std::function<bool()> Rule);
	bool SetStateRestartOnReentry(int32 StateIndex, bool bEnabled);
	bool Start(int32 InitialState);

	void Initialize(const FAnimationInitializeContext& Context) override;
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;

	int32 GetCurrentState() const { return StateMachine.GetCurrentState(); }
	FName GetCurrentStateName() const { return StateMachine.GetCurrentStateName(); }
	float GetStateElapsedTime() const { return StateMachine.GetStateElapsedTime(); }
	const FAnimStateMachine& GetStateMachine() const { return StateMachine; }
	const FAnimNode_Base* GetStatePoseNode(int32 StateIndex) const
	{
		return StateNodes.IsValidIndex(StateIndex) ? StateNodes[StateIndex].get() : nullptr;
	}

private:
	FAnimStateMachine StateMachine;
	TArray<TUniquePtr<FAnimNode_Base>> StateNodes;
	bool bInitialized = false;
};
