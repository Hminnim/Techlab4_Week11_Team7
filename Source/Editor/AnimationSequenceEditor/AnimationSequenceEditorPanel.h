#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Settings/SettingsPanel.h"

class Uworld;
class USkeletalMeshComponent;

// 애니메이션 시퀀스 에셋을 위한 독립 에디터 패널.
class FAnimationSequenceEditorPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Animation Sequence Editor"; }
	void SetEditorSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	bool IsRenderGrid() const { return bRenderGrid; }

	// EditorEngine이 프리뷰를 그릴 때 사용
	UWorld* GetPreviewWorld() const { return PreviewWorld; }
	// 엔진 종료시 월드를 안전하게 분리
	UWorld* ReleasePreviewWorld();

	FEditorViewportClient* GetViewportClient() { return &ViewportClient; }

private:
	void DrawViewport();
	void TickCameraInput();

	bool bRenderGrid = true;
	float CameraSpeed = 1.0f;

	UWorld* PreviewWorld = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	const FEditorSettings* Settings = nullptr;

	FEditorViewportClient ViewportClient;
};
