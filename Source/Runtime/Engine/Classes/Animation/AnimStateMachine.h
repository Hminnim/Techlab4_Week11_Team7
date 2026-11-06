#pragma once

#include "AnimStateMachineTypes.h"

class FAnimStateMachine
{
public:
	// 성공하면 상태 인덱스, 실패하면 -1 반환
	int32 AddState(FName Name);

	// 등록 순서가 전이 우선순위
	bool AddTransition(int32 FromState, int32 ToState, std::function<bool()> Rule);

	bool Start(int32 InitialState);
	void Update(float DeltaTime);

	int32 GetCurrentState() const { return CurrentState; }
	FName GetCurrentStateName() const;
	float GetStateElapsedTime() const { return ElapsedTime; }

private:
	bool IsValidState(int32 StateIndex) const;

	TArray<FAnimStateDesc> States;
	TArray<FAnimTransitionDesc> Transitions;

	int32 CurrentState = -1;
	float ElapsedTime = 0.0f;
	bool bStarted = false;
};
