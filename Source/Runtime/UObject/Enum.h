#pragma once

#include "Containers/Array.h"

struct FEnumEntry
{
	const char* Name;
	int64 Value;
	bool bHidden;
};

struct FEnumInfo
{
	const char* Name;
	TArray<FEnumEntry> Entries;
};

template <typename T>
struct TEnumInfo
{
	static constexpr bool bReflected = false;
};
