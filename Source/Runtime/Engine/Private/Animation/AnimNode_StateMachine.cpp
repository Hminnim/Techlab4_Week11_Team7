#include "EnginePCH.h"
#include "Engine/Classes/Animation/AnimNode_StateMachine.h"

#include <utility>

int32 FAnimNode_StateMachine::AddState(FName Name, TUniquePtr<FAnimNode_Base> Node)
{
	if (!Node)
	{
		return -1;
	}

	const int32 StateIndex = StateMachine.AddState(Name);
	if (StateIndex >= 0)
	{
		// 상태 번호와 노드 배열의 인덱스를 일치시킨다.
		StateNodes.Add(std::move(Node));
	}
	return StateIndex;
}

bool FAnimNode_StateMachine::AddTransition(int32 FromState, int32 ToState, std::function<bool()> Rule)
{
	return StateMachine.AddTransition(FromState, ToState, std::move(Rule));
}

bool FAnimNode_StateMachine::AddAnyStateTransition(int32 ToState, std::function<bool()> Rule)
{
	return StateMachine.AddAnyStateTransition(ToState, std::move(Rule));
}

bool FAnimNode_StateMachine::SetStateRestartOnReentry(int32 StateIndex, bool bEnabled)
{
	return StateMachine.SetStateRestartOnReentry(StateIndex, bEnabled);
}

bool FAnimNode_StateMachine::Start(int32 InitialState)
{
	return StateMachine.Start(InitialState);
}

void FAnimNode_StateMachine::Initialize(const FAnimationInitializeContext& Context)
{
	StateMachine.Reset();
	bInitialized = StateNodes.IsValidIndex(StateMachine.GetCurrentState());
	if (bInitialized)
	{
		StateNodes[StateMachine.GetCurrentState()]->Initialize(Context);
	}
}

void FAnimNode_StateMachine::Update(const FAnimationUpdateContext& Context)
{
	if (!bInitialized || Context.GetDeltaTime() <= 0.0f)
	{
		return;
	}

	if (StateMachine.Update(Context.GetDeltaTime()))
	{
		// 같은 상태로 재진입한 경우도 재생 노드를 다시 초기화한다.
		FAnimationInitializeContext InitializeContext(Context);
		StateNodes[StateMachine.GetCurrentState()]->Initialize(InitializeContext);
	}

	StateNodes[StateMachine.GetCurrentState()]->Update(Context);
}

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
	if (!bInitialized)
	{
		Output.Pose.ResetToRefPose();
		return;
	}

	StateNodes[StateMachine.GetCurrentState()]->Evaluate(Output);
}
