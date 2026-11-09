#pragma once

#include "LightweightStats.h"

namespace EditorStats
{
	DECLARE_CYCLE_STAT("Picking Time", STAT_PickingTime);
	DECLARE_CYCLE_STAT("Skinning CPU Update", STAT_SkinningCPUUpdate);
	DECLARE_CYCLE_STAT("Skinning GPU Prepare (CPU)", STAT_SkinningGPUPrepare);
}
