#pragma once

#include "Containers/Array.h"
#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/EditorUI/EditorTimeline.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/SkeletalMeshEditor/SkeletalPose.h"

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
	bool IsRenderBones() const { return SelectedBoneIndex >= 0; }
	int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }
	const FSkeletalPose& GetPreviewPose() const { return PreviewPose; }
	// Heatmap 모드와 본 선택을 분리해, 선택 해제 시에도 보라색 표시를 유지한다.
	bool IsBoneWeightEnabled() const;
	int32 GetBoneWeightBoneIndex() const;

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
	void DrawBoneHierarchy();
	void DrawBoneTreeNode(int32 BoneIndex);

	void DrawSequenceInfo();
	void DrawSelectedNotifyInspector();
	void DrawPlaybackControls();
	void DrawAnimationTimeline();
	void DrawNotifyTrack(ImDrawList* Draw, int32 TrackIndex, const ImVec2& Min, const ImVec2& Max, bool bCanHover);
	void DrawNotifyContextMenu();
	void DrawAddNotifyTrackPopup();
	int32 AddNotifyTrack(const char* InName);
	enum class ENotifyDragMode : uint8
	{
		None,
		Move,
		ResizeEnd
	};

	int32 HitTestNotify(float MouseX, float MouseY, int32 TrackIndex, const ImVec2& LaneMin, const ImVec2& LaneMax, ENotifyDragMode* OutPart = nullptr) const;
	int32 AddNotifyEvent(const char* InName, float InTriggerTime, float InDuration, int32 TrackIndex = INDEX_NONE);
	void SelectNotify(int32 Index);
	void TogglePlayback();
	void StepFrame(int32 Direction);
	void SetPreviewTime(float NewTime);
	void EvaluatePreviewPose();

	bool bRenderGrid = true;
	bool bShowBoneWeight = false;
	// 애니메이션의 트랙 순서가 아닌, 프리뷰 메시의 스켈레톤 인덱스를 사용한다.
	int32 SelectedBoneIndex = -1;
	TArray<TArray<int32>> BoneChildren;
	bool bRequestFocus = false;
	float CameraSpeed = 1.0f;

	UWorld* PreviewWorld = nullptr;
	USkeletalMeshComponent* PreviewMeshComponent = nullptr;
	FSkeletalPose PreviewPose;
	const FEditorSettings* Settings = nullptr;

	// AssetManager가 관리하는 에셋을 참조
	UAnimSequence* EditSequence = nullptr;
	float CurrentTime = 0.0f;
	bool bPlaying = false;
	bool bLooping = true;
	float PlayRate = 1.0f;
	EditorTimeline::FView TimelineView;
	bool bFitTimeline = true;

	// AnimNotify 편집 상태
	int32 SelectedNotifyIndex = INDEX_NONE;
	int32 SelectedNotifyTrackIndex = 0;
	int32 PendingNotifyTrackIndex = 0;
	bool bScrollToSelectedTrack = false;
	int32 DraggingNotifyIndex = INDEX_NONE;
	ENotifyDragMode DragMode = ENotifyDragMode::None;
	float PendingNotifyTime = 0.0f;
	float NotifyDragOffset = 0.0f;
	float NewNotifyDuration = 0.0f;
	char NewNotifyTrackName[128] = "";
	char NewNotifyName[128] = "Footstep";
	char EditNotifyName[128] = "";

	FEditorViewportClient ViewportClient;
};
