#pragma once

#include "AnimStateMachineTypes.h"

class FAnimStateMachine
{
public:
	// 성공하면 상태 인덱스, 실패하면 -1 반환
	int32 AddState(FName Name);

	// 등록 순서가 전이 우선순위
	bool AddTransition(int32 FromState, int32 ToState, std::function<bool()> Rule);
	// 현재 상태와 관계없이 일반 전이보다 먼저 검사한다.
	bool AddAnyStateTransition(int32 ToState, std::function<bool()> Rule);
	// 실행 중에도 변경 가능하며, 해당 상태를 향하는 Any State 전이에 적용한다.
	bool SetStateRestartOnReentry(int32 StateIndex, bool bEnabled);

	bool Start(int32 InitialState);
	void Reset();
	// 다른 상태로 전환하거나 현재 상태를 재시작했으면 true를 반환한다.
	bool Update(float DeltaTime);

	int32 GetCurrentState() const { return CurrentState; }
	int32 GetInitialState() const { return InitialState; }
	FName GetCurrentStateName() const;
	float GetStateElapsedTime() const { return ElapsedTime; }
	const TArray<FAnimStateDesc>& GetStates() const { return States; }
	const TArray<FAnimTransitionDesc>& GetTransitions() const { return Transitions; }

private:
	bool IsValidState(int32 StateIndex) const;

	TArray<FAnimStateDesc> States;
	TArray<FAnimTransitionDesc> Transitions;
	TArray<int32> AnyStateTransitions;

	int32 InitialState = -1;
	int32 CurrentState = -1;
	float ElapsedTime = 0.0f;
	bool bStarted = false;
};
