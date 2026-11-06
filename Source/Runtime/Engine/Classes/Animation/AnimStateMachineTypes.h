#pragma once

#include <functional>

#include "Core/Types.h"
#include "Core/NameTypes.h"
#include "Containers/Array.h"

struct FAnimStateDesc
{
	FName Name;
	TArray<int32> ExitTransitions;
	bool bRestartOnReentry = false;
};

struct FAnimTransitionDesc
{
	int32 FromState = -1;
	int32 ToState = -1;

	std::function<bool()> Rule;
};
