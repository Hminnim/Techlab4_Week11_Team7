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
#include "Render/SkeletalMesh.h"

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
		ImGui::Separator();
		DrawViewport();
	}

	ImGui::End();
	SetOpen(bPanelOpen);
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

	const FBox Bounds = PreviewMeshComponent->CalcBounds();
	ViewportClient.FocusBounds(Bounds);
	CameraSpeed = std::max(1.0f, (Bounds.Max - Bounds.Min).Length() * 0.5f);
	SetOpen(true);
	bRequestFocus = true;
	return true;
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
	EditSequence = nullptr;
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
