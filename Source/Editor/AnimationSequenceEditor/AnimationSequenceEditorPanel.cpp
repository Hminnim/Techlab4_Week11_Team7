#include "EnginePCH.h"
#include "Editor/AnimationSequenceEditor/AnimationSequenceEditorPanel.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/ViewportCameraMovement.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Engine/Classes/Animation/AnimSequence.h"
#include "Engine/Classes/Animation/BonePose.h"
#include "Asset/AssetManager.h"
#include "Render/SkeletalMesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>

bool FAnimationSequenceEditorPanel::Init()
{
	SetOpen(false);

	PreviewWorld = UWorld::CreateWorld(EWorldType::Editor, false, "AnimationSequencePreview");
	if (!PreviewWorld)
	{
		HTR_LOG(Error, "Failed to create preview world for animation sequence editor.");
		return false;
	}

	ViewportClient.SetWorld(PreviewWorld);
	ViewportClient.SetViewLocation(FVector(-4.0f, 0.0f, 2.0f));
	ViewportClient.SetViewRotation(FRotator(20.0f, 0.0f, 0.0f));

	if (AActor* Actor = PreviewWorld->SpawnActor<AActor>("AnimationPreviewMesh"))
	{
		PreviewMeshComponent = Cast<USkeletalMeshComponent>(Actor->AddComponent(USkeletalMeshComponent::StaticClass(), "PreviewMesh"));
	}

	if (AActor* Actor = PreviewWorld->SpawnActor<AActor>("AnimationPreviewLights"))
	{
		if (auto* Sky = Cast<USkyLightComponent>(Actor->AddComponent(USkyLightComponent::StaticClass(), "PreviewAmbient")))
		{
			Sky->SetIntensity(1.0f);
		}

		if (auto* Directional = Cast<UDirectionalLightComponent>(Actor->AddComponent(UDirectionalLightComponent::StaticClass(), "PreviewDirectional")))
		{
			Directional->SetRelativeRotation(FRotator(45.0f, 30.0f, 0.0f));
			Directional->SetIntensity(3.0f);
		}
	}


	return true;
}

void FAnimationSequenceEditorPanel::Tick(float DeltaTime)
{
	if (!IsOpen()) bPlaying = false;
	if (bPlaying && EditSequence && std::isfinite(DeltaTime) && DeltaTime > 0.0f)
	{
		const float Length = EditSequence->GetPlayLength();
		if (!std::isfinite(Length) || Length <= 0.0f)
		{
			bPlaying = false;
		}
		else
		{
			double NewTime = static_cast<double>(CurrentTime) + static_cast<double>(DeltaTime) * PlayRate;
			if (NewTime >= Length)
			{
				if (bLooping)
				{
					// 끝을 초과한 시간까지 유지하며 다음 반복으로 넘어간다.
					NewTime = std::fmod(NewTime, static_cast<double>(Length));
				}
				else
				{
					NewTime = Length;
					bPlaying = false;
				}
			}
			SetPreviewTime(static_cast<float>(NewTime));
		}
	}

	if (PreviewWorld)
	{
		PreviewWorld->GetScene().UpdateAllTransforms();
	}
}

void FAnimationSequenceEditorPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 700.0f), ImGuiCond_FirstUseEver);
	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
		ImGui::SetNextWindowCollapsed(false);
		bRequestFocus = false;
	}

	bool bPanelOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bPanelOpen))
	{
		ImGui::Checkbox("Grid", &bRenderGrid);
		ImGui::SameLine();
		const bool bCanShowBoneWeight = USkeletalMeshComponent::IsGPUSkinningEnabled() &&
			PreviewMeshComponent && PreviewMeshComponent->GetSkeletalMesh();
		ImGui::BeginDisabled(!bCanShowBoneWeight);
		ImGui::Checkbox("Bone Weight", &bShowBoneWeight);
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip(bCanShowBoneWeight
				? "Select a bone in the hierarchy to view its weights."
				: "Requires GPU skinning and a skeletal mesh.");
		DrawSequenceInfo();

		const ImGuiStyle& Style = ImGui::GetStyle();
		const float AvailableHeight = ImGui::GetContentRegionAvail().y;
		const float TimelineHeight = std::clamp(AvailableHeight * 0.35f, 110.0f, 220.0f);
		const float TransportHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f + Style.WindowPadding.y * 2.0f;
		const float PreviewHeight = std::max(1.0f, AvailableHeight - TimelineHeight - TransportHeight - Style.ItemSpacing.y * 2.0f);
		const ImGuiWindowFlags ChildFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		if (ImGui::BeginChild("##AnimationPreview", { 0.0f, PreviewHeight }, ImGuiChildFlags_Borders, ChildFlags))
		{
			// 계층만 스크롤하고, 타임라인과 재생 컨트롤은 프리뷰 아래에 그대로 둔다.
			if (ImGui::BeginTable("##AnimationPreviewLayout", 2,
				ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Skeleton", ImGuiTableColumnFlags_WidthFixed, 220.0f);
				ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextColumn();
				if (ImGui::BeginChild("##AnimationBoneHierarchy", { 0.0f, 0.0f },
					ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
					DrawBoneHierarchy();
				ImGui::EndChild();
				ImGui::TableNextColumn();
				DrawViewport();
				ImGui::EndTable();
			}
		}
		ImGui::EndChild();

		if (ImGui::BeginChild("##AnimationTimeline", { 0.0f, TimelineHeight }, ImGuiChildFlags_Borders, ChildFlags))
			DrawAnimationTimeline();
		ImGui::EndChild();

		if (ImGui::BeginChild("##AnimationTransport", { 0.0f, TransportHeight }, ImGuiChildFlags_Borders, ChildFlags))
			DrawPlaybackControls();
		ImGui::EndChild();
	}

	ImGui::End();
	SetOpen(bPanelOpen);
	if (!bPanelOpen) bPlaying = false;
}

// 콘텐츠 브라우저에서 선택한 클립과 같은 FBX의 메시를 연결한다.
bool FAnimationSequenceEditorPanel::OpenAsset(UAnimSequence* InSequence, USkeletalMesh* InSkeletalMesh)
{
	if (!PreviewWorld || !PreviewMeshComponent || !InSequence || !InSkeletalMesh)
	{
		HTR_LOG(Error, "Cannot open animation sequence: preview or asset is missing.");
		return false;
	}

	const UAnimDataModel* Model = InSequence->GetDataModel();
	if (!Model || Model->GetNumberOfKeys() <= 0 || InSkeletalMesh->GetRefSkeleton().RawRefBoneInfo.IsEmpty())
	{
		HTR_LOG(Error, "Cannot open animation sequence '{}': animation keys or mesh skeleton are missing.", InSequence->GetPath());
		return false;
	}

	PreviewMeshComponent->SetSkeletalMesh(InSkeletalMesh);
	if (PreviewMeshComponent->GetSkeletalMesh() != InSkeletalMesh) return false;
	// 클립/메시 교체 시 이전 본 선택을 지우고, 변하지 않는 계층은 한 번만 구성한다.
	SelectedBoneIndex = -1;
	const auto& Bones = InSkeletalMesh->GetRefSkeleton().RawRefBoneInfo;
	BoneChildren.Reset();
	BoneChildren.SetNum(Bones.Num());
	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
		if (BoneChildren.IsValidIndex(ParentIndex))
			BoneChildren[ParentIndex].Add(BoneIndex);
	}
	PreviewMeshComponent->ResetToReferencePose();
	EditSequence = InSequence;
	bPlaying = false;
	SelectedNotifyIndex = INDEX_NONE;
	DraggingNotifyIndex = INDEX_NONE;
	DragMode = ENotifyDragMode::None;
	EditNotifyName[0] = '\0';
	SetPreviewTime(0.0f);
	bFitTimeline = true;

	const FBox Bounds = PreviewMeshComponent->CalcBounds();
	ViewportClient.FocusBounds(Bounds);
	CameraSpeed = std::max(1.0f, (Bounds.Max - Bounds.Min).Length() * 0.5f);
	SetOpen(true);
	bRequestFocus = true;
	return true;
}

void FAnimationSequenceEditorPanel::DrawPlaybackControls()
{
	if (!EditSequence)
	{
		ImGui::TextDisabled("Open an animation from the Content Drawer.");
		return;
	}
	const float Length = EditSequence->GetPlayLength();
	if (!std::isfinite(Length) || Length < 0.0f) return;
	if (!ImGui::BeginTable("##PlaybackControls", 2, ImGuiTableFlags_SizingStretchSame)) return;

	ImGui::TableNextColumn();
	if (ImGui::Button("|<"))
	{
		bPlaying = false;
		SetPreviewTime(0.0f);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go to start");
	ImGui::SameLine();
	if (ImGui::Button("<")) StepFrame(-1);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous frame");
	ImGui::SameLine();
	ImGui::BeginDisabled(Length <= 0.0f);
	if (ImGui::Button(bPlaying ? "Pause" : "Play", { 55.0f, 0.0f })) TogglePlayback();
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button(">")) StepFrame(+1);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next frame");
	ImGui::SameLine();
	if (ImGui::Button(">|"))
	{
		bPlaying = false;
		SetPreviewTime(Length);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go to end");

	ImGui::TableNextColumn();
	ImGui::Checkbox("Loop", &bLooping);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(70.0f);
	ImGui::DragFloat("Speed", &PlayRate, 0.05f, 0.1f, 4.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);

	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth(100.0f);
	float EditedTime = CurrentTime;
	const bool bInputChanged = ImGui::InputFloat("Time (sec)", &EditedTime, 0.01f, 0.1f, "%.3f");
	if (ImGui::IsItemActivated() || bInputChanged) bPlaying = false;
	if (bInputChanged) SetPreviewTime(EditedTime);

	ImGui::TableNextColumn();
	if (ImGui::Button("Fit")) bFitTimeline = true;
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fit the entire animation in the timeline");
	ImGui::SameLine();
	const double FPS = EditSequence->GetSamplingFrameRate().AsDecimal();
	if (std::isfinite(FPS) && FPS > 0.0)
		ImGui::Text("Frame %.0f / %d", std::round(CurrentTime * FPS), std::max(0, EditSequence->GetNumberOfSampledKeys() - 1));
	ImGui::EndTable();
}

void FAnimationSequenceEditorPanel::DrawAnimationTimeline()
{
	if (!EditSequence)
	{
		ImGui::TextDisabled("No animation sequence selected.");
		return;
	}
	const float Length = EditSequence->GetPlayLength();
	const double FPS = EditSequence->GetSamplingFrameRate().AsDecimal();
	if (!std::isfinite(Length) || Length <= 0.0f || !std::isfinite(FPS) || FPS <= 0.0)
	{
		ImGui::TextDisabled("No timeline range available.");
		return;
	}

	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	const float LabelWidth = std::min(160.0f, Size.x * 0.3f);
	constexpr float RulerHeight = 26.0f;
	constexpr float HeaderHeight = 24.0f;
	constexpr float LaneHeight = 40.0f;
	if (Size.x <= LabelWidth + 16.0f || Size.y < RulerHeight + HeaderHeight + LaneHeight) return;

	const ImVec2 AxisMin{ Origin.x + LabelWidth, Origin.y };
	const ImVec2 AxisMax{ Origin.x + Size.x, Origin.y + Size.y };
	const float AxisWidth = AxisMax.x - AxisMin.x;
	const float FitScale = AxisWidth / Length;
	if (bFitTimeline)
	{
		TimelineView.StartTime = 0.0f;
		TimelineView.PixelsPerSecond = FitScale;
		bFitTimeline = false;
	}
	TimelineView.PixelsPerSecond = std::max(TimelineView.PixelsPerSecond, FitScale);

	const float HeaderY = Origin.y + RulerHeight;
	const float LaneTop = HeaderY + HeaderHeight;
	const float LaneBottom = LaneTop + LaneHeight;
	const ImVec2 LaneMin{ AxisMin.x, LaneTop };
	const ImVec2 LaneMax{ AxisMax.x, LaneBottom };

	ImGui::SetCursorScreenPos(AxisMin);
	ImGui::InvisibleButton("##AnimationTimelineInput", { AxisWidth, Size.y });
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bInputHovered = ImGui::IsItemHovered();
	const bool bInputActive = ImGui::IsItemActive();

	if (bInputHovered)
	{
		if (IO.KeyCtrl && IO.MouseWheel != 0.0f)
			TimelineView.ZoomAt(IO.MouseWheel > 0.0f ? 1.15f : 1.0f / 1.15f, IO.MousePos.x, AxisMin.x, FitScale, FitScale * 100.0f);
		else if (IO.KeyShift && IO.MouseWheel != 0.0f)
			TimelineView.PanPixels(-IO.MouseWheel * 40.0f);
	}
	const float VisibleDuration = AxisWidth / TimelineView.PixelsPerSecond;
	TimelineView.StartTime = std::clamp(TimelineView.StartTime, 0.0f, std::max(0.0f, Length - VisibleDuration));

	const bool bInNotifyLane = (IO.MousePos.y >= LaneTop && IO.MousePos.y <= LaneBottom);

	// 호버 시 리사이즈 핸들 커서 표시
	if (DraggingNotifyIndex == INDEX_NONE && bInputHovered && bInNotifyLane)
	{
		ENotifyDragMode HoverPart = ENotifyDragMode::None;
		if (HitTestNotify(IO.MousePos.x, IO.MousePos.y, LaneMin, LaneMax, &HoverPart) != INDEX_NONE &&
			HoverPart == ENotifyDragMode::ResizeEnd)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
	}

	// 좌클릭 시작: 노티파이 트랙 내 마커/구간 핸들 히트 테스트
	if (bInputHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (bInNotifyLane)
		{
			ENotifyDragMode HitPart = ENotifyDragMode::None;
			const int32 HitIdx = HitTestNotify(IO.MousePos.x, IO.MousePos.y, LaneMin, LaneMax, &HitPart);
			if (HitIdx != INDEX_NONE)
			{
				SelectNotify(HitIdx);
				DraggingNotifyIndex = HitIdx;
				DragMode = HitPart;
			}
			else
			{
				SelectNotify(INDEX_NONE);
				DraggingNotifyIndex = INDEX_NONE;
				DragMode = ENotifyDragMode::None;
			}
		}
		else
		{
			DraggingNotifyIndex = INDEX_NONE;
			DragMode = ENotifyDragMode::None;
		}
	}

	// 좌클릭 드래그: 노티파이 이동, 구간 끝 리사이즈, 또는 플레이헤드 스크러빙
	if (bInputActive && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		if (DraggingNotifyIndex != INDEX_NONE && EditSequence->Notifies.IsValidIndex(DraggingNotifyIndex))
		{
			FAnimNotifyEvent& DragNotify = EditSequence->Notifies[DraggingNotifyIndex];
			float MouseTime = std::clamp(TimelineView.XToTime(IO.MousePos.x, AxisMin.x), 0.0f, Length);
			if (IO.KeyShift)
			{
				MouseTime = static_cast<float>(std::round(MouseTime * FPS) / FPS);
			}

			if (DragMode == ENotifyDragMode::ResizeEnd)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
				const float MinDur = std::min(0.02f, std::max(0.0f, Length - DragNotify.TriggerTime));
				const float EndTime = std::clamp(MouseTime, DragNotify.TriggerTime + MinDur, Length);
				DragNotify.Duration = std::max(0.0f, EndTime - DragNotify.TriggerTime);
			}
			else
			{
				const float MaxStart = std::max(0.0f, Length - DragNotify.Duration);
				DragNotify.TriggerTime = std::clamp(MouseTime, 0.0f, MaxStart);
			}
			UAssetManager::Get().SyncAnimNotifiesByClipName(EditSequence);
		}
		else
		{
			bPlaying = false;
			const float MouseTime = TimelineView.XToTime(IO.MousePos.x, AxisMin.x);
			SetPreviewTime(static_cast<float>(std::round(MouseTime * FPS) / FPS));
		}
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		DraggingNotifyIndex = INDEX_NONE;
		DragMode = ENotifyDragMode::None;
	}

	// 우클릭: 컨텍스트 메뉴 오픈 (노티파이 추가/삭제)
	if (bInputHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		if (bInNotifyLane)
		{
			const int32 HitIdx = HitTestNotify(IO.MousePos.x, IO.MousePos.y, LaneMin, LaneMax);
			if (HitIdx != INDEX_NONE)
			{
				SelectNotify(HitIdx);
			}
		}
		PendingNotifyTime = std::clamp(TimelineView.XToTime(IO.MousePos.x, AxisMin.x), 0.0f, Length);
		ImGui::OpenPopup("##NotifyContextPopup");
	}

	// Delete 키 단축키: 선택된 노티파이 삭제
	if (!IO.WantTextInput && EditSequence->Notifies.IsValidIndex(SelectedNotifyIndex) && ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		EditSequence->Notifies.RemoveAt(SelectedNotifyIndex);
		UAssetManager::Get().SyncAnimNotifiesByClipName(EditSequence);
		SelectNotify(INDEX_NONE);
	}

	ImDrawList* Draw = ImGui::GetWindowDrawList();
	Draw->AddRectFilled(Origin, AxisMax, IM_COL32(28, 30, 33, 255));
	Draw->AddRectFilled(Origin, { AxisMin.x, AxisMax.y }, IM_COL32(35, 38, 42, 255));
	Draw->AddText({ Origin.x + 6.0f, Origin.y + 5.0f }, IM_COL32(180, 180, 180, 255), "Tracks");
	EditorTimeline::DrawRuler(Draw, TimelineView, AxisMin, { AxisMax.x, Origin.y + RulerHeight }, AxisMax.y, FPS, true, true);

	Draw->AddRectFilled({ Origin.x, HeaderY }, { AxisMax.x, LaneTop }, IM_COL32(45, 48, 52, 255));
	char TrackLabel[64];
	std::snprintf(TrackLabel, sizeof(TrackLabel), "Notifies (%d)", EditSequence->Notifies.Num());
	Draw->AddText({ Origin.x + 6.0f, HeaderY + 4.0f }, IM_COL32(220, 220, 220, 255), TrackLabel);
	Draw->AddText({ Origin.x + 18.0f, LaneTop + 10.0f }, IM_COL32(190, 190, 190, 255), "1");
	Draw->AddLine({ Origin.x, LaneBottom }, { AxisMax.x, LaneBottom }, IM_COL32(65, 68, 72, 255));
	Draw->AddLine({ AxisMin.x, Origin.y }, { AxisMin.x, AxisMax.y }, IM_COL32(65, 68, 72, 255));

	Draw->PushClipRect(LaneMin, LaneMax, true);
	DrawNotifyTrack(Draw, LaneMin, LaneMax);
	Draw->PopClipRect();
	EditorTimeline::DrawPlayhead(Draw, TimelineView, CurrentTime, AxisMin, AxisMax);

	if (ImGui::BeginPopup("##NotifyContextPopup"))
	{
		ImGui::Text("Time: %.3f sec", PendingNotifyTime);
		ImGui::Separator();
		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputText("Name##PopupNotify", NewNotifyName, sizeof(NewNotifyName));
		ImGui::SetNextItemWidth(160.0f);
		ImGui::DragFloat("Dur##PopupNotify", &NewNotifyDuration, 0.01f, 0.0f, std::max(0.0f, Length - PendingNotifyTime), "%.2fs (0=Point)", ImGuiSliderFlags_AlwaysClamp);

		if (ImGui::Button("Add Notify"))
		{
			AddNotifyEvent(NewNotifyName, PendingNotifyTime, NewNotifyDuration);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Add State (0.2s)"))
		{
			AddNotifyEvent(NewNotifyName, PendingNotifyTime, 0.2f);
			ImGui::CloseCurrentPopup();
		}

		ImGui::Separator();
		ImGui::TextDisabled("Quick Presets");
		if (ImGui::MenuItem("Add 'Footstep' (Point)"))
		{
			AddNotifyEvent("Footstep", PendingNotifyTime, 0.0f);
		}
		if (ImGui::MenuItem("Add 'Attack_Slash' (Point)"))
		{
			AddNotifyEvent("Attack_Slash", PendingNotifyTime, 0.0f);
		}
		if (ImGui::MenuItem("Add 'Attack_Window' (State 0.25s)"))
		{
			AddNotifyEvent("Attack_Window", PendingNotifyTime, 0.25f);
		}
		if (ImGui::MenuItem("Add 'Walk_StepWindow' (State 0.20s)"))
		{
			AddNotifyEvent("Walk_StepWindow", PendingNotifyTime, 0.20f);
		}

		if (EditSequence->Notifies.IsValidIndex(SelectedNotifyIndex))
		{
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Selected Notify"))
			{
				EditSequence->Notifies.RemoveAt(SelectedNotifyIndex);
				UAssetManager::Get().SyncAnimNotifiesByClipName(EditSequence);
				SelectNotify(INDEX_NONE);
			}
		}
		ImGui::EndPopup();
	}
}

int32 FAnimationSequenceEditorPanel::AddNotifyEvent(const char* InName, float InTriggerTime, float InDuration)
{
	if (!EditSequence) return INDEX_NONE;
	const float Length = EditSequence->GetPlayLength();
	FAnimNotifyEvent NewEvent;
	NewEvent.Duration = std::max(0.0f, std::min(InDuration, Length));
	NewEvent.TriggerTime = std::clamp(InTriggerTime, 0.0f, std::max(0.0f, Length - NewEvent.Duration));
	NewEvent.NotifyName = FName((InName && InName[0] != '\0') ? InName : "AnimNotify");
	const int32 NewIdx = EditSequence->Notifies.Add(NewEvent);
	UAssetManager::Get().SyncAnimNotifiesByClipName(EditSequence);
	SelectNotify(NewIdx);
	return NewIdx;
}

int32 FAnimationSequenceEditorPanel::HitTestNotify(float MouseX, float MouseY, const ImVec2& LaneMin, const ImVec2& LaneMax, ENotifyDragMode* OutPart) const
{
	if (OutPart) *OutPart = ENotifyDragMode::None;
	if (!EditSequence) return INDEX_NONE;
	const float Length = EditSequence->GetPlayLength();
	const float CenterY = LaneMin.y + 12.0f;

	for (int32 i = EditSequence->Notifies.Num() - 1; i >= 0; --i)
	{
		const FAnimNotifyEvent& Notify = EditSequence->Notifies[i];
		if (!std::isfinite(Notify.TriggerTime) || !std::isfinite(Notify.Duration) ||
			Notify.TriggerTime < 0.0f || Notify.TriggerTime > Length || Notify.Duration < 0.0f) continue;

		const float StartX = TimelineView.TimeToX(Notify.TriggerTime, LaneMin.x);
		const float EndTime = static_cast<float>(std::min(static_cast<double>(Length), static_cast<double>(Notify.TriggerTime) + Notify.Duration));
		const float EndX = TimelineView.TimeToX(EndTime, LaneMin.x);

		const float HitLeft = std::max(LaneMin.x, StartX - 7.0f);
		const float HitRight = std::min(LaneMax.x, std::max(EndX + 4.0f, StartX + 7.0f));
		const float HitTop = CenterY - 9.0f;
		const float HitBottom = CenterY + 9.0f;

		if (MouseX >= HitLeft && MouseX <= HitRight && MouseY >= HitTop && MouseY <= HitBottom)
		{
			if (OutPart)
			{
				const bool bOnRightGrip = (Notify.Duration > 0.0f) && (EndX - StartX >= 8.0f) && (std::fabs(MouseX - EndX) <= 6.0f);
				*OutPart = bOnRightGrip ? ENotifyDragMode::ResizeEnd : ENotifyDragMode::Move;
			}
			return i;
		}
	}
	return INDEX_NONE;
}

void FAnimationSequenceEditorPanel::SelectNotify(int32 Index)
{
	if (!EditSequence || !EditSequence->Notifies.IsValidIndex(Index))
	{
		SelectedNotifyIndex = INDEX_NONE;
		EditNotifyName[0] = '\0';
		return;
	}

	SelectedNotifyIndex = Index;
	const FString Name = EditSequence->Notifies[Index].NotifyName.ToString();
	std::snprintf(EditNotifyName, sizeof(EditNotifyName), "%s", Name.c_str());
}

void FAnimationSequenceEditorPanel::DrawNotifyTrack(ImDrawList* Draw, const ImVec2& Min, const ImVec2& Max)
{
	if (EditSequence->Notifies.IsEmpty())
	{
		Draw->AddText({ Min.x + 8.0f, Min.y + 10.0f }, IM_COL32(130, 130, 130, 255), "Right-click to add AnimNotify");
		return;
	}
	const float Length = EditSequence->GetPlayLength();
	const float CenterY = Min.y + 12.0f;
	const bool bCanHover = ImGui::IsItemHovered();
	bool bTooltipShown = false;
	for (int32 i = 0; i < EditSequence->Notifies.Num(); ++i)
	{
		const FAnimNotifyEvent& Notify = EditSequence->Notifies[i];
		if (!std::isfinite(Notify.TriggerTime) || !std::isfinite(Notify.Duration) ||
			Notify.TriggerTime < 0.0f || Notify.TriggerTime > Length || Notify.Duration < 0.0f) continue;

		const float StartX = TimelineView.TimeToX(Notify.TriggerTime, Min.x);
		const float EndTime = static_cast<float>(std::min(static_cast<double>(Length), static_cast<double>(Notify.TriggerTime) + Notify.Duration));
		const float EndX = TimelineView.TimeToX(EndTime, Min.x);
		if (Notify.Duration > 0.0f)
		{
			if (EndX < Min.x || StartX > Max.x) continue;
		}
		else if (StartX < Min.x - 6.0f || StartX > Max.x + 6.0f) continue;

		const FString Name = Notify.NotifyName.ToString();
		const char* Label = Name.empty() ? "(Unnamed)" : Name.c_str();
		const bool bSelected = (i == SelectedNotifyIndex);
		const ImU32 Color = bSelected ? IM_COL32(255, 195, 50, 255) : IM_COL32(100, 180, 240, 255);

		Draw->AddLine({ StartX, Min.y }, { StartX, Max.y },
			bSelected ? IM_COL32(255, 195, 50, 140) : IM_COL32(100, 180, 240, 80), 1.0f);

		if (Notify.Duration > 0.0f)
		{
			const float Left = std::clamp(StartX, Min.x, Max.x - 2.0f);
			const float Right = std::clamp(EndX, Left + 2.0f, Max.x);
			const ImU32 StateColor = bSelected ? IM_COL32(255, 185, 45, 220) : IM_COL32(75, 175, 130, 210);
			Draw->AddRectFilled({ Left, CenterY - 7.0f }, { Right, CenterY + 7.0f }, StateColor, 2.0f);
			if (EndX >= Min.x && EndX <= Max.x)
			{
				Draw->AddRectFilled({ Right - 3.0f, CenterY - 7.0f }, { Right, CenterY + 7.0f }, IM_COL32(235, 245, 255, 220), 1.0f);
			}
			if (bSelected)
			{
				Draw->AddRect({ Left, CenterY - 7.0f }, { Right, CenterY + 7.0f }, IM_COL32(255, 255, 255, 255), 2.0f, 0, 1.5f);
			}
		}
		else
		{
			Draw->AddQuadFilled({ StartX, CenterY - 6.0f }, { StartX + 6.0f, CenterY },
				{ StartX, CenterY + 6.0f }, { StartX - 6.0f, CenterY }, Color);
			if (bSelected)
			{
				Draw->AddQuad({ StartX, CenterY - 6.0f }, { StartX + 6.0f, CenterY },
					{ StartX, CenterY + 6.0f }, { StartX - 6.0f, CenterY }, IM_COL32(255, 255, 255, 255), 1.5f);
			}
		}
		Draw->AddText({ StartX + 8.0f, CenterY + 10.0f },
			bSelected ? IM_COL32(255, 230, 130, 255) : IM_COL32(220, 220, 220, 255), Label);

		const ImVec2 HitMin{ std::max(Min.x, StartX - 6.0f), CenterY - 8.0f };
		const ImVec2 HitMax{ std::min(Max.x, std::max(EndX + 4.0f, StartX + 6.0f)), CenterY + 8.0f };
		if (bCanHover && !bTooltipShown && ImGui::IsMouseHoveringRect(HitMin, HitMax))
		{
			if (Notify.IsNotifyState())
			{
				ImGui::SetTooltip("%s [NotifyState]\nStart: %.3f sec | Dur: %.3f sec | End: %.3f sec\n(Drag bar to move, Drag right edge to resize, Shift to snap)",
					Label, Notify.TriggerTime, Notify.Duration, EndTime);
			}
			else
			{
				ImGui::SetTooltip("%s [Notify]\nTime: %.3f sec\n(Drag to move, Shift+Drag to snap)", Label, Notify.TriggerTime);
			}
			bTooltipShown = true;
		}
	}
}

void FAnimationSequenceEditorPanel::TogglePlayback()
{
	if (!EditSequence) return;
	const float Length = EditSequence->GetPlayLength();
	if (!std::isfinite(Length) || Length <= 0.0f) return;

	if (!bPlaying && CurrentTime >= Length) SetPreviewTime(0.0f);
	bPlaying = !bPlaying;
}

void FAnimationSequenceEditorPanel::StepFrame(int32 Direction)
{
	if (!EditSequence) return;
	bPlaying = false;
	const double FPS = EditSequence->GetSamplingFrameRate().AsDecimal();
	if (!std::isfinite(FPS) || FPS <= 0.0) return;

	const double CurrentFrame = std::round(static_cast<double>(CurrentTime) * FPS);
	SetPreviewTime(static_cast<float>((CurrentFrame + Direction) / FPS));
}

void FAnimationSequenceEditorPanel::SetPreviewTime(float NewTime)
{
	if (!EditSequence || !std::isfinite(NewTime)) return;
	const float Length = EditSequence->GetPlayLength();
	if (!std::isfinite(Length) || Length < 0.0f) return;

	CurrentTime = std::clamp(NewTime, 0.0f, Length);
	EvaluatePreviewPose();
}

void FAnimationSequenceEditorPanel::EvaluatePreviewPose()
{
	if (!EditSequence || !PreviewMeshComponent) return;
	USkeletalMesh* Mesh = PreviewMeshComponent->GetSkeletalMesh();
	if (!Mesh) return;

	// 메시의 스켈레톤을 기준으로 현재 시간의 본별 로컬 자세를 구한다.
	FCompactPose Pose;
	Pose.SetRefSkeleton(&Mesh->GetRefSkeleton());
	FAnimExtractContext Context;
	Context.CurrentTime = CurrentTime;
	Context.bLooping = false;
	EditSequence->GetAnimationPose(Pose, Context);

	if (Pose.GetNumBones() != PreviewMeshComponent->GetNumBones()) return;
	for (int32 BoneIndex = 0; BoneIndex < Pose.GetNumBones(); ++BoneIndex)
	{
		PreviewMeshComponent->SetBoneLocalTransform(BoneIndex, Pose[BoneIndex]);
	}
}

void FAnimationSequenceEditorPanel::DrawSequenceInfo()
{
	if (!EditSequence)
	{
		ImGui::TextUnformatted("No animation sequence selected.");
		return;
	}

	const FString& Key = EditSequence->GetPath();
	const size_t Separator = Key.find_last_of('/');
	const FString Name = Separator == FString::npos ? Key : Key.substr(Separator + 1);
	ImGui::Text("Animation: %s", Name.c_str());
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Key.c_str());
	ImGui::SameLine(0.0f, 16.0f);
	ImGui::TextDisabled("Length: %.3f sec | FPS: %.2f | Keys: %d",
		EditSequence->GetPlayLength(), EditSequence->GetSamplingFrameRate().AsDecimal(), EditSequence->GetNumberOfSampledKeys());

	DrawSelectedNotifyInspector();
}

void FAnimationSequenceEditorPanel::DrawSelectedNotifyInspector()
{
	if (!EditSequence) return;

	const float Length = EditSequence->GetPlayLength();

	ImGui::SetNextItemWidth(120.0f);
	ImGui::InputText("##QuickNotifyName", NewNotifyName, sizeof(NewNotifyName));
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("New AnimNotify Name");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(65.0f);
	ImGui::DragFloat("##QuickNotifyDur", &NewNotifyDuration, 0.01f, 0.0f, Length, "%.2fs", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Duration (0.00 = Point Notify, > 0 = Notify State)");
	ImGui::SameLine();
	if (ImGui::Button("+ Add @ Playhead"))
	{
		AddNotifyEvent(NewNotifyName, CurrentTime, NewNotifyDuration);
	}

	if (EditSequence->Notifies.IsValidIndex(SelectedNotifyIndex))
	{
		FAnimNotifyEvent& Notify = EditSequence->Notifies[SelectedNotifyIndex];
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.2f, 1.0f), "Selected [%d]:", SelectedNotifyIndex);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputText("Name##EditNotify", EditNotifyName, sizeof(EditNotifyName)))
		{
			Notify.NotifyName = FName((EditNotifyName[0] != '\0') ? EditNotifyName : "AnimNotify");
			UAssetManager::Get().SyncAnimNotifiesByClipName(EditSequence);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(75.0f);
		const float MaxStartTime = std::max(0.0f, Length - Notify.Duration);
		if (ImGui::DragFloat("Time##EditNotify", &Notify.TriggerTime, 0.005f, 0.0f, MaxStartTime, "%.3fs", ImGuiSliderFlags_AlwaysClamp))
		{
			UAssetManager::Get().SyncAnimNotifiesByClipName(EditSequence);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(75.0f);
		if (ImGui::DragFloat("Dur##EditNotify", &Notify.Duration, 0.005f, 0.0f, std::max(0.0f, Length - Notify.TriggerTime), "%.3fs", ImGuiSliderFlags_AlwaysClamp))
		{
			UAssetManager::Get().SyncAnimNotifiesByClipName(EditSequence);
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete##EditNotify"))
		{
			EditSequence->Notifies.RemoveAt(SelectedNotifyIndex);
			UAssetManager::Get().SyncAnimNotifiesByClipName(EditSequence);
			SelectNotify(INDEX_NONE);
		}
	}
	else
	{
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::TextDisabled("(Right-click Notify track to add / Left-click & drag marker to move)");
	}
}

bool FAnimationSequenceEditorPanel::IsBoneWeightEnabled() const
{
	return bShowBoneWeight && USkeletalMeshComponent::IsGPUSkinningEnabled() &&
		PreviewMeshComponent && PreviewMeshComponent->GetSkeletalMesh();
}

int32 FAnimationSequenceEditorPanel::GetBoneWeightBoneIndex() const
{
	if (!IsBoneWeightEnabled() ||
		!PreviewMeshComponent->GetSkeletalMesh()->GetRefSkeleton().RawRefBoneInfo.IsValidIndex(SelectedBoneIndex))
		return -1;
	return SelectedBoneIndex;
}

void FAnimationSequenceEditorPanel::DrawBoneHierarchy()
{
	ImGui::SeparatorText("Bone Hierarchy");
	const USkeletalMesh* Mesh = PreviewMeshComponent ? PreviewMeshComponent->GetSkeletalMesh() : nullptr;
	if (!Mesh)
	{
		ImGui::TextDisabled("Open an animation to view its skeleton.");
		return;
	}

	ImGui::BeginDisabled(SelectedBoneIndex < 0);
	if (ImGui::Button("Clear Selection")) SelectedBoneIndex = -1;
	ImGui::EndDisabled();

	const auto& Bones = Mesh->GetRefSkeleton().RawRefBoneInfo;
	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		if (Bones[BoneIndex].ParentIndex < 0)
			DrawBoneTreeNode(BoneIndex);
	}
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		SelectedBoneIndex = -1;
}

void FAnimationSequenceEditorPanel::DrawBoneTreeNode(int32 BoneIndex)
{
	const USkeletalMesh* Mesh = PreviewMeshComponent ? PreviewMeshComponent->GetSkeletalMesh() : nullptr;
	if (!Mesh || !BoneChildren.IsValidIndex(BoneIndex)) return;
	const auto& Bones = Mesh->GetRefSkeleton().RawRefBoneInfo;
	if (!Bones.IsValidIndex(BoneIndex)) return;

	const bool bIsLeaf = BoneChildren[BoneIndex].IsEmpty();
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if (bIsLeaf) Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (BoneIndex == SelectedBoneIndex) Flags |= ImGuiTreeNodeFlags_Selected;

	const FString Name = Bones[BoneIndex].Name.ToString();
	ImGui::PushID(BoneIndex);
	const bool bOpen = ImGui::TreeNodeEx("##Bone", Flags, "%s", Name.c_str());
	ImGui::PopID();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (Index: %d)", Name.c_str(), BoneIndex);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		SelectedBoneIndex = BoneIndex;

	if (bOpen && !bIsLeaf)
	{
		for (int32 ChildIndex : BoneChildren[BoneIndex])
			DrawBoneTreeNode(ChildIndex);
		ImGui::TreePop();
	}
}

UWorld* FAnimationSequenceEditorPanel::ReleasePreviewWorld()
{
	// 월드를 삭제하기 전에 관찰 참조부터 해제
	ViewportClient.SetWorld(nullptr);
	bPlaying = false;
	EditSequence = nullptr;
	SelectedNotifyIndex = INDEX_NONE;
	DraggingNotifyIndex = INDEX_NONE;
	DragMode = ENotifyDragMode::None;
	EditNotifyName[0] = '\0';
	CurrentTime = 0.0f;
	TimelineView = {};
	bFitTimeline = true;
	SelectedBoneIndex = -1;
	BoneChildren.Reset();
	PreviewMeshComponent = nullptr;

	UWorld* ReleasedWorld = PreviewWorld;
	PreviewWorld = nullptr;

	return ReleasedWorld;
}

// 프리뷰 캐릭터를 보여주는 뷰포트 영역을 그립니다.
void FAnimationSequenceEditorPanel::DrawViewport()
{
	if (IsBoneWeightEnabled())
	{
		const int32 BoneIndex = GetBoneWeightBoneIndex();
		if (BoneIndex >= 0)
			ImGui::TextWrapped("Bone Weight: %s",
				PreviewMeshComponent->GetSkeletalMesh()->GetRefSkeleton().RawRefBoneInfo[BoneIndex].Name.ToString().c_str());
		else
			ImGui::TextDisabled("No bone selected: Weight 0 (Purple).");
		ImGui::TextDisabled("0 Purple > Blue > Green > Yellow > Red 1");
	}

	const ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	const uint32 Width = static_cast<uint32>(std::max(1.0f, ViewportSize.x));
	const uint32 Height = static_cast<uint32>(std::max(1.0f, ViewportSize.y));
	if (Width != ViewportClient.GetWidth() || Height != ViewportClient.GetHeight())
	{
		ViewportClient.Resize(Width, Height);
	}
	if (ViewportClient.GetColorTarget() && ViewportClient.GetColorTarget()->GetSRV())
	{
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const ImVec2 Size(static_cast<float>(Width), static_cast<float>(Height));
		ImGui::InvisibleButton("##AnimationPreviewViewport", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		ImGui::GetWindowDrawList()->AddImage(ViewportClient.GetColorTarget()->GetSRV(), Min, ImVec2(Min.x + Size.x, Min.y + Size.y));
		TickCameraInput(); 
		// 프리뷰 클릭은 선택만 해제한다. Heatmap 모드는 유지해 메시를 보라색으로 표시한다.
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			SelectedBoneIndex = -1;
	}
}

void FAnimationSequenceEditorPanel::TickCameraInput()
{
	if (ImGui::IsItemActivated())
		ImGui::SetWindowFocus();
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bCaptured = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Right) ;

	FViewportCameraInput Input{
		.bCaptured = bCaptured,
		.bAllowWheel = bCaptured || ImGui::IsItemHovered(),
		.bForward = ImGui::IsKeyDown(ImGuiKey_W),
		.bBackward = ImGui::IsKeyDown(ImGuiKey_S),
		.bRight = ImGui::IsKeyDown(ImGuiKey_D),
		.bLeft = ImGui::IsKeyDown(ImGuiKey_A),
		.bUp = ImGui::IsKeyDown(ImGuiKey_E),
		.bDown = ImGui::IsKeyDown(ImGuiKey_Q),
		.MouseX = static_cast<float>(IO.MouseDelta.x),
		.MouseY = static_cast<float>(IO.MouseDelta.y),
		.Wheel = static_cast<float>(IO.MouseWheel) * 120.0f,
	};

	ViewportClient.ApplyCameraInput(Input, IO.DeltaTime, CameraSpeed * Settings->CameraSpeed, 0.2f * Settings->MouseSensitivity);
}
