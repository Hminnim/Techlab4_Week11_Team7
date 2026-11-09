#pragma once

#include "Core/EngineString.h"
#include "Core/Types.h"
#include "Containers/Array.h"

struct FAnimSequenceImportData;

class FFbxAnimBake
{
public:
	static FString GetBakePath(const FString& FbxPath);

	static bool ReadBaked(const FString& FbxPath, TArray<FAnimSequenceImportData>& OutAnimations);
	static bool WriteBaked(const FString& FbxPath, const TArray<FAnimSequenceImportData>& Animations);
};