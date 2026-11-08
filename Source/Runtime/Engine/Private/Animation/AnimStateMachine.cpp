#include "EnginePCH.h"
#include "Engine/Classes/Animation/AnimStateMachine.h"

#include <utility>

int32 FAnimStateMachine::AddState(FName Name)
{
	// 실행 시작 후에는 구성을 변경하지 않음
	if (bStarted || Name.IsNone())
	{
		return -1;
	}

	// 상태 이름 중복 금지
	for (const FAnimStateDesc& State : States)
	{
		if (State.Name == Name)
		{
			return -1;
		}
	}

	FAnimStateDesc State;
	State.Name = Name;

	return static_cast<int32>(States.Add(std::move(State)));
}

bool FAnimStateMachine::AddTransition(int32 FromState, int32 ToState, std::function<bool()> Rule)
{
	if (bStarted || FromState == ToState || !Rule || !IsValidState(FromState) || !IsValidState(ToState))
	{
		return false;
	}

	FAnimTransitionDesc Transition;
	Transition.FromState = FromState;
	Transition.ToState = ToState;
	Transition.Rule = std::move(Rule);

	const int32 TransitionIndex = static_cast<int32>(Transitions.Add(std::move(Transition)));
	States[FromState].ExitTransitions.Add(TransitionIndex);
	return true;
}

bool FAnimStateMachine::AddAnyStateTransition(int32 ToState, std::function<bool()> Rule)
{
	if (bStarted || !Rule || !IsValidState(ToState))
	{
		return false;
	}

	FAnimTransitionDesc Transition;
	Transition.FromState = -1; // 특정 출발 상태가 없는 공통 전이
	Transition.ToState = ToState;
	Transition.Rule = std::move(Rule);

	const int32 TransitionIndex = static_cast<int32>(Transitions.Add(std::move(Transition)));
	AnyStateTransitions.Add(TransitionIndex);
	return true;
}

bool FAnimStateMachine::SetStateRestartOnReentry(int32 StateIndex, bool bEnabled)
{
	if (!IsValidState(StateIndex))
	{
		return false;
	}

	States[StateIndex].bRestartOnReentry = bEnabled;
	return true;
}

bool FAnimStateMachine::Start(int32 InInitialState)
{
	if (bStarted || !IsValidState(InInitialState))
	{
		return false;
	}

	InitialState = InInitialState;
	bStarted = true;
	Reset();
	return true;
}

void FAnimStateMachine::Reset()
{
	CurrentState = InitialState;
	ElapsedTime = 0.0f;
}

bool FAnimStateMachine::Update(float DeltaTime)
{
	if (!bStarted || DeltaTime <= 0.0f)
	{
		return false;
	}

	ElapsedTime += DeltaTime;

	// 공통 전이를 먼저 검사한다. 재진입 시 대상 상태의 재시작 설정을 따른다.
	for (int32 TransitionIndex : AnyStateTransitions)
	{
		const FAnimTransitionDesc& Transition = Transitions[TransitionIndex];
		if (Transition.ToState == CurrentState && !States[CurrentState].bRestartOnReentry)
		{
			continue;
		}

		if (Transition.Rule && Transition.Rule())
		{
			CurrentState = Transition.ToState;
			ElapsedTime = 0.0f;
			return true;
		}
	}

	// 규칙을 평가할 상태를 고정
	const int32 SourceState = CurrentState;

	for (int32 TransitionIndex : States[SourceState].ExitTransitions)
	{
		const FAnimTransitionDesc& Transition = Transitions[TransitionIndex];

		if (Transition.Rule && Transition.Rule())
		{
			CurrentState = Transition.ToState;
			ElapsedTime = 0.0f;

			// 첫 버전은 Update당 한 번만 전환
			return true;
		}
	}

	return false;
}

FName FAnimStateMachine::GetCurrentStateName() const
{
	if (!IsValidState(CurrentState))
	{
		return NAME_None;
	}

	return States[CurrentState].Name;
}

bool FAnimStateMachine::IsValidState(int32 StateIndex) const
{
	return StateIndex >= 0 && StateIndex < States.Num();
}
