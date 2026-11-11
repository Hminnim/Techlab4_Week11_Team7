#pragma once

#include "Asset/AssetManager.h"
#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"

#include <functional>

class AActor;
class UWorld;
class USkeletalMesh;
class USkeletalMeshComponent;
class UBlendSpaceAnimInstance;
struct FAnimNode_BlendSpacePlayer;
struct FAnimNode_TwoWayBlend;

// BlendSpace 1D 및 Two-Way Blend 인터랙티브 에디터/시연 패널
class FAnimationBlendEditorPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "BlendSpace & Two-Way Blend"; }

	void SetEditorSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	void SetSelectedActorQuery(std::function<AActor*()> Query) { SelectedActorQuery = std::move(Query); }
	void SetFallbackPreviewQuery(std::function<UBlendSpaceAnimInstance*()> Query) { FallbackPreviewQuery = std::move(Query); }

	void OpenBlendSpaceTab();
	void OpenTwoWayBlendTab();

	bool IsRenderGrid() const { return bRenderGrid; }
	UWorld* GetPreviewWorld() const { return PreviewWorld; }
	UWorld* ReleasePreviewWorld();
	FEditorViewportClient* GetViewportClient() { return &ViewportClient; }

private:
	enum class EEditorTab : uint8
	{
		BlendSpace1D,
		TwoWayBlend
	};

	enum class ETargetSource : uint8
	{
		StandalonePreview,
		SceneOrPIEActor
	};

	struct FBlendTarget
	{
		FAnimNode_BlendSpacePlayer* BlendSpacePlayer = nullptr;
		float* SpeedPtr = nullptr;
		FAnimNode_TwoWayBlend* TwoWayBlend = nullptr;
		float* TwoWayAlphaPtr = nullptr;
		class UCharacterAnimInstance* CharacterInst = nullptr;
		FString Label = "Standalone Preview";
		bool bIsSceneTarget = false;
	};

	bool EnsureStandalonePreviewReady();
	void ResetDefaultBlendSpaceSamples();
	FBlendTarget ResolveTarget();

	void DrawTargetToolbar(const FBlendTarget& Target);
	void DrawPreviewViewport(float ReservedBottomHeight);
	void TickCameraInput();

	void DrawBlendSpaceTab(const FBlendTarget& Target);
	void DrawBlendSpaceCanvas(FAnimNode_BlendSpacePlayer& BSPlayer, float* SpeedPtr);
	void DrawBlendSpaceDetails(FAnimNode_BlendSpacePlayer& BSPlayer, float* SpeedPtr, class UCharacterAnimInstance* CharInst);

	void DrawTwoWayBlendTab(const FBlendTarget& Target);
	void DrawTwoWayClipSelector(const char* Label, int32& InOutClipIndex, struct FAnimNode_SequencePlayer& Player, UWorld* WorldPtr);

	UWorld* PreviewWorld = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	class UBlendDemoAnimInstance* StandaloneAnimInst = nullptr;
	USkeletalMesh* DefaultMesh = nullptr;
	const FEditorSettings* Settings = nullptr;
	std::function<AActor*()> SelectedActorQuery;
	std::function<UBlendSpaceAnimInstance*()> FallbackPreviewQuery;

	FEditorViewportClient ViewportClient;
	float CameraSpeed = 1.0f;
	bool bRenderGrid = true;
	bool bPlaying = true;
	float PreviewPlayRate = 1.0f;
	bool bRequestFocus = false;
	bool bPendingTabSwitch = false;
	EEditorTab RequestedTab = EEditorTab::BlendSpace1D;
	EEditorTab ActiveTab = EEditorTab::BlendSpace1D;
	ETargetSource PreferredSource = ETargetSource::StandalonePreview;

	// BlendSpace 편집 상태
	TArray<FFbxAnimationAsset> AvailableClips;
	int32 SelectedSampleIndex = INDEX_NONE;
	int32 DraggingSampleIndex = INDEX_NONE;
	bool bDraggingCursor = false;
	int32 AddSampleClipIndex = 0;
	float AddSampleX = 250.0f;

	// Two-Way Blend 데모 상태
	int32 ClipAIndex = 0;
	int32 ClipBIndex = 0;
	bool bUseBlendSpaceForA = false;
	bool bAutoPingPongAlpha = false;
	float PingPongSpeed = 0.8f;
	float PingPongDir = 1.0f;
};
