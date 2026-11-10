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

	bool bPanelOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bPanelOpen))
	{
		ImGui::Checkbox("Grid", &bRenderGrid);
		DrawViewport();
	}

	ImGui::End();
	SetOpen(bPanelOpen);
}

UWorld* FAnimationSequenceEditorPanel::ReleasePreviewWorld()
{
	// 월드를 삭제하기 전에 관찰 참조부터 해제
	ViewportClient.SetWorld(nullptr);
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
