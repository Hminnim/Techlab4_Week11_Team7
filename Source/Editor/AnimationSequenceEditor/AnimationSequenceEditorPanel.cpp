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
#include "Render/SkeletalMesh.h"

#include <algorithm>
#include <cmath>

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
		DrawSequenceInfo();

		const ImGuiStyle& Style = ImGui::GetStyle();
		const float AvailableHeight = ImGui::GetContentRegionAvail().y;
		const float TimelineHeight = std::clamp(AvailableHeight * 0.35f, 110.0f, 220.0f);
		const float TransportHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f + Style.WindowPadding.y * 2.0f;
		const float PreviewHeight = std::max(1.0f, AvailableHeight - TimelineHeight - TransportHeight - Style.ItemSpacing.y * 2.0f);
		const ImGuiWindowFlags ChildFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		if (ImGui::BeginChild("##AnimationPreview", { 0.0f, PreviewHeight }, ImGuiChildFlags_Borders, ChildFlags))
			DrawViewport();
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
	PreviewMeshComponent->ResetToReferencePose();
	EditSequence = InSequence;
	bPlaying = false;
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

	ImGui::SetCursorScreenPos(AxisMin);
	ImGui::InvisibleButton("##AnimationTimelineInput", { AxisWidth, Size.y });
	const ImGuiIO& IO = ImGui::GetIO();
	if (ImGui::IsItemHovered())
	{
		if (IO.KeyCtrl && IO.MouseWheel != 0.0f)
			TimelineView.ZoomAt(IO.MouseWheel > 0.0f ? 1.15f : 1.0f / 1.15f, IO.MousePos.x, AxisMin.x, FitScale, FitScale * 100.0f);
		else if (IO.KeyShift && IO.MouseWheel != 0.0f)
			TimelineView.PanPixels(-IO.MouseWheel * 40.0f);
	}
	const float VisibleDuration = AxisWidth / TimelineView.PixelsPerSecond;
	TimelineView.StartTime = std::clamp(TimelineView.StartTime, 0.0f, std::max(0.0f, Length - VisibleDuration));

	if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		bPlaying = false;
		const float MouseTime = TimelineView.XToTime(IO.MousePos.x, AxisMin.x);
		SetPreviewTime(static_cast<float>(std::round(MouseTime * FPS) / FPS));
	}

	ImDrawList* Draw = ImGui::GetWindowDrawList();
	Draw->AddRectFilled(Origin, AxisMax, IM_COL32(28, 30, 33, 255));
	Draw->AddRectFilled(Origin, { AxisMin.x, AxisMax.y }, IM_COL32(35, 38, 42, 255));
	Draw->AddText({ Origin.x + 6.0f, Origin.y + 5.0f }, IM_COL32(180, 180, 180, 255), "Tracks");
	EditorTimeline::DrawRuler(Draw, TimelineView, AxisMin, { AxisMax.x, Origin.y + RulerHeight }, AxisMax.y, FPS, true, true);

	const float HeaderY = Origin.y + RulerHeight;
	const float LaneTop = HeaderY + HeaderHeight;
	const float LaneBottom = LaneTop + LaneHeight;
	Draw->AddRectFilled({ Origin.x, HeaderY }, { AxisMax.x, LaneTop }, IM_COL32(45, 48, 52, 255));
	char TrackLabel[64];
	std::snprintf(TrackLabel, sizeof(TrackLabel), "Notifies (%d)", EditSequence->Notifies.Num());
	Draw->AddText({ Origin.x + 6.0f, HeaderY + 4.0f }, IM_COL32(220, 220, 220, 255), TrackLabel);
	Draw->AddText({ Origin.x + 18.0f, LaneTop + 10.0f }, IM_COL32(190, 190, 190, 255), "1");
	Draw->AddLine({ Origin.x, LaneBottom }, { AxisMax.x, LaneBottom }, IM_COL32(65, 68, 72, 255));
	Draw->AddLine({ AxisMin.x, Origin.y }, { AxisMin.x, AxisMax.y }, IM_COL32(65, 68, 72, 255));

	Draw->PushClipRect({ AxisMin.x, LaneTop }, { AxisMax.x, LaneBottom }, true);
	DrawNotifyTrack(Draw, { AxisMin.x, LaneTop }, { AxisMax.x, LaneBottom });
	Draw->PopClipRect();
	EditorTimeline::DrawPlayhead(Draw, TimelineView, CurrentTime, AxisMin, AxisMax);
}

void FAnimationSequenceEditorPanel::DrawNotifyTrack(ImDrawList* Draw, const ImVec2& Min, const ImVec2& Max)
{
	if (EditSequence->Notifies.IsEmpty())
	{
		Draw->AddText({ Min.x + 8.0f, Min.y + 10.0f }, IM_COL32(130, 130, 130, 255), "No notifies");
		return;
	}
	const float Length = EditSequence->GetPlayLength();
	const float CenterY = Min.y + 12.0f;
	const bool bCanHover = ImGui::IsItemHovered();
	bool bTooltipShown = false;
	for (const FAnimNotifyEvent& Notify : EditSequence->Notifies)
	{
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
		const ImU32 Color = IM_COL32(100, 180, 240, 255);
		if (Notify.Duration > 0.0f)
		{
			const float Left = std::clamp(StartX, Min.x, Max.x - 2.0f);
			const float Right = std::clamp(EndX, Left + 2.0f, Max.x);
			Draw->AddRectFilled({ Left, CenterY - 7.0f }, { Right, CenterY + 7.0f }, Color, 2.0f);
		}
		else
		{
			Draw->AddQuadFilled({ StartX, CenterY - 6.0f }, { StartX + 6.0f, CenterY },
				{ StartX, CenterY + 6.0f }, { StartX - 6.0f, CenterY }, Color);
		}
		Draw->AddText({ StartX + 8.0f, CenterY + 10.0f }, IM_COL32(220, 220, 220, 255), Label);

		const ImVec2 HitMin{ std::max(Min.x, StartX - 6.0f), CenterY - 8.0f };
		const ImVec2 HitMax{ std::min(Max.x, std::max(EndX, StartX + 6.0f)), CenterY + 8.0f };
		if (bCanHover && !bTooltipShown && ImGui::IsMouseHoveringRect(HitMin, HitMax))
		{
			ImGui::SetTooltip("%s\nTime: %.3f sec\nDuration: %.3f sec", Label, Notify.TriggerTime, Notify.Duration);
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
	ImGui::Text("Length: %.3f sec | FPS: %.2f | Sampled Keys: %d",
		EditSequence->GetPlayLength(), EditSequence->GetSamplingFrameRate().AsDecimal(), EditSequence->GetNumberOfSampledKeys());
}

UWorld* FAnimationSequenceEditorPanel::ReleasePreviewWorld()
{
	// 월드를 삭제하기 전에 관찰 참조부터 해제
	ViewportClient.SetWorld(nullptr);
	bPlaying = false;
	EditSequence = nullptr;
	CurrentTime = 0.0f;
	TimelineView = {};
	bFitTimeline = true;
	PreviewMeshComponent = nullptr;

	UWorld* ReleasedWorld = PreviewWorld;
	PreviewWorld = nullptr;

	return ReleasedWorld;
}

// 프리뷰 캐릭터를 보여주는 뷰포트 영역을 그립니다.
void FAnimationSequenceEditorPanel::DrawViewport()
{
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
