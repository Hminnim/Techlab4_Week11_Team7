#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Settings/SettingsPanel.h"

class USkeletalMeshComponent;
class UWorld;
class USkeletalMesh;
class UAnimSequence;

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

	// 콘텐츠 브라우저에서 선택한 클립과 같은 FBX의 메시를 연결한다.
	bool OpenAsset(UAnimSequence* InSequence, USkeletalMesh* InSkeletalMesh);

	// EditorEngine이 프리뷰를 그릴 때 사용
	UWorld* GetPreviewWorld() const { return PreviewWorld; }
	// 엔진 종료시 월드를 안전하게 분리
	UWorld* ReleasePreviewWorld();

	FEditorViewportClient* GetViewportClient() { return &ViewportClient; }

private:
	void DrawViewport();
	void TickCameraInput();

	void DrawSequenceInfo();

	bool bRenderGrid = true;
	bool bRequestFocus = false;
	float CameraSpeed = 1.0f;

	UWorld* PreviewWorld = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	const FEditorSettings* Settings = nullptr;

	// AssetManager가 관리하는 에셋을 참조
	UAnimSequence* EditSequence = nullptr;

	FEditorViewportClient ViewportClient;
};
