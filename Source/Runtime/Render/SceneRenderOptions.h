#pragma once

#include "Core/Types.h"
#include "Render/RenderEnums.h"

// 뷰 하나의 씬 렌더링 옵션. 에디터와 게임 뷰포트가 같은 설정을 전달한다.
struct FSceneRenderOptions
{
	bool bDrawPrimitives = true;
	bool bShowSkeletalMesh = true;
	EPcfMode PcfMode = EPcfMode::Sample8;
	float ShadowDepthBias = 0.0001f;
	bool bCacheShadows = true; // r.Shadow.CacheWholeSceneShadows: 바뀌지 않은 그림자 타일은 다시 그리지 않는다
	bool bEnableFog = true;
	bool bEnableFXAA = false;
	ELightCullingMode LightCullingMode = ELightCullingMode::Tiled;
	bool bShowSceneDepth = false;
	bool bShowCascade = false;
	bool bShowShadowMask = false;   // 언리얼 구조 그림자의 ScreenShadowMask를 화면에 띄운다 (Deferred)
	bool bShowWorldNormal = false;
	// SkeletalMesh/Animation Sequence 프리뷰용. 선택된 본이 없어도 표시 모드는 유지한다.
	bool bShowBoneWeight = false;
	// -1이면 선택 없음(Weight 0), 0 이상이면 해당 본의 Weight를 표시한다.
	int32 BoneWeightBoneIndex = -1;
	bool bShowLightCulling = false;	
	float SceneDepthRange = 1.0f;
	uint32 DirectionalShadowMapSize = 0;
	uint32 PointShadowMapSize = 0;
	uint32 SpotShadowMapSize = 0;

	bool bGammaCorrection = true;
	float DisplayGamma = 2.2f;
	bool bShowVignette = true;
	bool bShowCameraOverlay = true;
	bool bShowDecal = true;

	ERenderingPath RenderingPath = ERenderingPath::Deferred;
	ELightingModel LightingModel = ELightingModel::Gouraud;
};
