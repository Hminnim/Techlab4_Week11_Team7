#pragma once

#include "Asset/RenderAsset.h"
#include "AnimationAsset.generated.h"

UCLASS()
class UAnimationAsset : public URenderAsset
{
	GENERATED_BODY()

public:
	virtual float GetPlayLength() const { return 0.0f; }
};
