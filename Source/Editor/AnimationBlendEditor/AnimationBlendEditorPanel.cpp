#include "EnginePCH.h"
#include "Editor/AnimationBlendEditor/AnimationBlendEditorPanel.h"

#include "Asset/AssetManager.h"
#include "Camera/ViewportCameraMovement.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Classes/Animation/AnimNode_BlendSpacePlayer.h"
#include "Engine/Classes/Animation/AnimNode_SequencePlayer.h"
#include "Engine/Classes/Animation/AnimNode_TwoWayBlend.h"
#include "Engine/Classes/Animation/AnimSequence.h"
#include "Engine/Classes/Animation/BlendSpace.h"
#include "Engine/Classes/Animation/BlendSpaceAnimInstance.h"
#include "Engine/Classes/Animation/CharacterAnimInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Render/SkeletalMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

class UBlendDemoAnimInstance : public UAnimInstance
{
public:
	enum class EMode : uint8
	{
		BlendSpace1D,
		TwoWayBlend
	};

	FAnimNode_BlendSpacePlayer BlendSpacePlayer;
	FAnimNode_SequencePlayer TwoWayPlayerA;
	FAnimNode_BlendSpacePlayer TwoWayBlendSpaceA;
	FAnimNode_SequencePlayer TwoWayPlayerB;
	FAnimNode_TwoWayBlend TwoWayBlend;

	float BlendSpaceSpeed = 0.0f;
	float TwoWayAlpha = 0.0f;
	bool bUseBlendSpaceForA = false;
	EMode Mode = EMode::BlendSpace1D;

	UBlendDemoAnimInstance()
	{
		TwoWayBlend.A.SetLinkNode(&TwoWayPlayerA);
		TwoWayBlend.B.SetLinkNode(&TwoWayPlayerB);
		RootNode = &BlendSpacePlayer;
	}

	void SetMode(EMode InMode)
	{
		if (Mode == InMode) return;
		Mode = InMode;
		RootNode = (Mode == EMode::BlendSpace1D)
			? static_cast<FAnimNode_Base*>(&BlendSpacePlayer)
			: static_cast<FAnimNode_Base*>(&TwoWayBlend);
	}

	void SetUseBlendSpaceForA(bool bEnable)
	{
		bUseBlendSpaceForA = bEnable && (TwoWayBlendSpaceA.BlendSpace != nullptr);
		TwoWayBlend.A.SetLinkNode(bUseBlendSpaceForA
			? static_cast<FAnimNode_Base*>(&TwoWayBlendSpaceA)
			: static_cast<FAnimNode_Base*>(&TwoWayPlayerA));
	}

	void NativeUpdateAnimation(float DeltaSeconds) override
	{
		(void)DeltaSeconds;
		BlendSpacePlayer.X = BlendSpaceSpeed;
		TwoWayBlendSpaceA.X = BlendSpaceSpeed;
		TwoWayBlend.Alpha = FMath::Clamp(TwoWayAlpha, 0.0f, 1.0f);
	}
};

namespace
{
	constexpr const char* DefaultUalMeshKey = "Models/UAL1/UAL1_Standard.fbx";
	constexpr const char* DefaultUalMeshPath = "Assets/Models/UAL1/UAL1_Standard.fbx";

	FString ExtractShortClipName(const UAnimSequenceBase* Seq)
	{
		if (!Seq) return "None";
		const FString& Path = Seq->GetPath();
		const FString& Source = Path.empty() ? Seq->GetName() : Path;
		const size_t Sep = Source.find_last_of("/\\");
		return (Sep != FString::npos) ? Source.substr(Sep + 1) : Source;
	}

	int32 FindClipIndexBySuffix(const TArray<FFbxAnimationAsset>& Clips, const FString& Suffix)
	{
		for (int32 i = 0; i < Clips.Num(); ++i)
		{
			if (Clips[i].DisplayName == Suffix || Clips[i].DisplayName.ends_with(Suffix))
			{
				return i;
			}
		}
		return Clips.IsEmpty() ? INDEX_NONE : 0;
	}

	void ComputeBlendSpaceLengths(const UBlendSpace* BS, const TArray<FBlendSampleData>& Cache, float& OutHarmonic, float& OutArithmetic)
	{
		OutHarmonic = BS ? BS->GetAnimationLengthFromSampleData(Cache) : 0.0f;
		OutArithmetic = 0.0f;
		if (!BS) return;

		for (const FBlendSampleData& Data : Cache)
		{
			if (!BS->SampleData.IsValidIndex(Data.SampleDataIndex)) continue;
			const float Len = BS->GetBlendSample(Data.SampleDataIndex).GetSamplePlayLength();
			OutArithmetic += Len * Data.TotalWeight;
		}
	}
}

bool FAnimationBlendEditorPanel::Init()
{
	SetOpen(false);

	PreviewWorld = UWorld::CreateWorld(EWorldType::Editor, false, "AnimationBlendPreview");
	if (!PreviewWorld)
	{
		HTR_LOG(Error, "Failed to create preview world for BlendSpace & Two-Way Blend editor.");
		return false;
	}

	ViewportClient.SetWorld(PreviewWorld);
	ViewportClient.SetViewLocation(FVector(-3.5f, 0.0f, 1.5f));
	ViewportClient.SetViewRotation(FRotator(15.0f, 0.0f, 0.0f));

	if (AActor* MeshActor = PreviewWorld->SpawnActor<AActor>("AnimationBlendPreviewMesh"))
	{
		PreviewMeshComponent = Cast<USkeletalMeshComponent>(
			MeshActor->AddComponent(USkeletalMeshComponent::StaticClass(), "PreviewMesh"));
	}

	if (AActor* LightActor = PreviewWorld->SpawnActor<AActor>("AnimationBlendPreviewLights"))
	{
		if (auto* Sky = Cast<USkyLightComponent>(LightActor->AddComponent(USkyLightComponent::StaticClass(), "PreviewAmbient")))
		{
			Sky->SetIntensity(1.0f);
		}
		if (auto* Dir = Cast<UDirectionalLightComponent>(LightActor->AddComponent(UDirectionalLightComponent::StaticClass(), "PreviewDirectional")))
		{
			Dir->SetRelativeRotation(FRotator(45.0f, 30.0f, 0.0f));
			Dir->SetIntensity(3.0f);
		}
	}

	return true;
}

bool FAnimationBlendEditorPanel::EnsureStandalonePreviewReady()
{
	if (StandaloneAnimInst && DefaultMesh && PreviewMeshComponent && PreviewMeshComponent->GetSkeletalMesh())
	{
		return true;
	}

	if (!PreviewWorld || !PreviewMeshComponent) return false;

	FString Error;
	DefaultMesh = UAssetManager::Get().LoadFbxSkeletalMesh(DefaultUalMeshKey, DefaultUalMeshPath, Error);
	if (!DefaultMesh) return false;

	PreviewMeshComponent->SetSkeletalMesh(DefaultMesh);
	UAssetManager::Get().GetFbxAnimationAssets(DefaultUalMeshKey, AvailableClips);

	auto AnimInst = MakeUnique<UBlendDemoAnimInstance>();
	StandaloneAnimInst = AnimInst.get();

	ResetDefaultBlendSpaceSamples();

	// Two-Way Blend 기본 클립: Walk_Loop <-> Sword_Attack (존재하는 클립 자동 선택)
	ClipAIndex = FindClipIndexBySuffix(AvailableClips, "Walk_Loop");
	ClipBIndex = FindClipIndexBySuffix(AvailableClips, "Sword_Attack");
	if (AvailableClips.IsValidIndex(ClipAIndex))
	{
		AnimInst->TwoWayPlayerA.SetSequence(AvailableClips[ClipAIndex].Sequence);
		AnimInst->TwoWayPlayerA.SetLoopAnimation(true);
	}
	if (AvailableClips.IsValidIndex(ClipBIndex))
	{
		AnimInst->TwoWayPlayerB.SetSequence(AvailableClips[ClipBIndex].Sequence);
		AnimInst->TwoWayPlayerB.SetLoopAnimation(true);
	}

	// BlendSpace를 Two-Way A 입력으로도 쓸 수 있게 복제 구성
	auto* TwoWayBS = NewObject<UBlendSpace>();
	if (AnimInst->BlendSpacePlayer.BlendSpace)
	{
		for (const FBlendSample& Sample : AnimInst->BlendSpacePlayer.BlendSpace->SampleData)
		{
			TwoWayBS->AddSample(Sample.Animation, Sample.SamepleVector);
		}
	}
	AnimInst->TwoWayBlendSpaceA.BlendSpace.reset(TwoWayBS);
	AnimInst->SetUseBlendSpaceForA(bUseBlendSpaceForA);

	PreviewMeshComponent->SetAnimInstance(std::move(AnimInst));

	const FBox Bounds = PreviewMeshComponent->CalcBounds();
	ViewportClient.FocusBounds(Bounds);
	CameraSpeed = std::max(1.0f, (Bounds.Max - Bounds.Min).Length() * 0.5f);
	return true;
}

void FAnimationBlendEditorPanel::ResetDefaultBlendSpaceSamples()
{
	if (!StandaloneAnimInst) return;

	FString Error;
	UAnimSequence* Idle = UAssetManager::Get().LoadFbxAnimSequence(DefaultUalMeshPath, Error, "Idle_Loop");
	UAnimSequence* Walk = UAssetManager::Get().LoadFbxAnimSequence(DefaultUalMeshPath, Error, "Walk_Loop");
	UAnimSequence* Jog = UAssetManager::Get().LoadFbxAnimSequence(DefaultUalMeshPath, Error, "Jog_Fwd_Loop");
	UAnimSequence* Sprint = UAssetManager::Get().LoadFbxAnimSequence(DefaultUalMeshPath, Error, "Sprint_Loop");

	UBlendSpace* BS = NewObject<UBlendSpace>();
	if (Idle) BS->AddSample(Idle, FVector(0.0f, 0.0f, 0.0f));
	if (Walk) BS->AddSample(Walk, FVector(150.0f, 0.0f, 0.0f));
	if (Jog) BS->AddSample(Jog, FVector(350.0f, 0.0f, 0.0f));
	if (Sprint) BS->AddSample(Sprint, FVector(600.0f, 0.0f, 0.0f));

	StandaloneAnimInst->BlendSpacePlayer.BlendSpace.reset(BS);
	StandaloneAnimInst->BlendSpacePlayer.InternalTimeAccumulator = 0.0f;
	SelectedSampleIndex = INDEX_NONE;
	DraggingSampleIndex = INDEX_NONE;
}

FAnimationBlendEditorPanel::FBlendTarget FAnimationBlendEditorPanel::ResolveTarget()
{
	FBlendTarget Result;

	if (PreferredSource == ETargetSource::SceneOrPIEActor)
	{
		AActor* Selected = SelectedActorQuery ? SelectedActorQuery() : nullptr;
		USkeletalMeshComponent* SkelComp = nullptr;
		if (Selected)
		{
			for (UActorComponent* Candidate : Selected->GetComponents())
			{
				if ((SkelComp = Cast<USkeletalMeshComponent>(Candidate))) break;
			}
		}

		if (SkelComp && SkelComp->GetAnimInstance())
		{
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
			if (auto* BSInst = dynamic_cast<UBlendSpaceAnimInstance*>(AnimInst))
			{
				Result.BlendSpacePlayer = &BSInst->BlendSpacePlayer;
				Result.SpeedPtr = &BSInst->Speed;
				Result.Label = Selected->GetName() + " (UBlendSpaceAnimInstance)";
				Result.bIsSceneTarget = true;
				return Result;
			}
			if (auto* TWInst = dynamic_cast<UTwoWayBlendAnimInstance*>(AnimInst))
			{
				Result.TwoWayBlend = &TWInst->TwoWayBlend;
				Result.TwoWayAlphaPtr = &TWInst->Alpha;
				Result.Label = Selected->GetName() + " (UTwoWayBlendAnimInstance)";
				Result.bIsSceneTarget = true;
				return Result;
			}
			if (auto* CharInst = Cast<UCharacterAnimInstance>(AnimInst))
			{
				Result.CharacterInst = CharInst;
				Result.BlendSpacePlayer = CharInst->FindBlendSpacePlayer();
				Result.TwoWayBlend = CharInst->FindTwoWayBlend();
				if (Result.BlendSpacePlayer)
				{
					Result.SpeedPtr = &Result.BlendSpacePlayer->X;
				}
				if (Result.TwoWayBlend)
				{
					Result.TwoWayAlphaPtr = &Result.TwoWayBlend->Alpha;
				}
				Result.Label = Selected->GetName() + " (UCharacterAnimInstance)";
				Result.bIsSceneTarget = true;
				return Result;
			}
		}

		if (UBlendSpaceAnimInstance* Fallback = FallbackPreviewQuery ? FallbackPreviewQuery() : nullptr)
		{
			Result.BlendSpacePlayer = &Fallback->BlendSpacePlayer;
			Result.SpeedPtr = &Fallback->Speed;
			Result.Label = "Scene BlendSpace Preview (F8)";
			Result.bIsSceneTarget = true;
			return Result;
		}
	}

	if (EnsureStandalonePreviewReady() && StandaloneAnimInst)
	{
		Result.BlendSpacePlayer = &StandaloneAnimInst->BlendSpacePlayer;
		Result.SpeedPtr = &StandaloneAnimInst->BlendSpaceSpeed;
		Result.TwoWayBlend = &StandaloneAnimInst->TwoWayBlend;
		Result.TwoWayAlphaPtr = &StandaloneAnimInst->TwoWayAlpha;
		Result.Label = "Standalone Preview";
		Result.bIsSceneTarget = false;
	}
	return Result;
}

void FAnimationBlendEditorPanel::OpenBlendSpaceTab()
{
	SetOpen(true);
	bRequestFocus = true;
	RequestedTab = EEditorTab::BlendSpace1D;
	bPendingTabSwitch = true;
}

void FAnimationBlendEditorPanel::OpenTwoWayBlendTab()
{
	SetOpen(true);
	bRequestFocus = true;
	RequestedTab = EEditorTab::TwoWayBlend;
	bPendingTabSwitch = true;
}

void FAnimationBlendEditorPanel::Tick(float DeltaTime)
{
	if (!IsOpen()) return;
	if (!EnsureStandalonePreviewReady()) return;

	if (bAutoPingPongAlpha && std::isfinite(DeltaTime) && DeltaTime > 0.0f)
	{
		const FBlendTarget Target = ResolveTarget();
		if (Target.TwoWayAlphaPtr)
		{
			*Target.TwoWayAlphaPtr += PingPongDir * PingPongSpeed * DeltaTime;
			if (*Target.TwoWayAlphaPtr >= 1.0f)
			{
				*Target.TwoWayAlphaPtr = 1.0f;
				PingPongDir = -1.0f;
			}
			else if (*Target.TwoWayAlphaPtr <= 0.0f)
			{
				*Target.TwoWayAlphaPtr = 0.0f;
				PingPongDir = 1.0f;
			}
		}
	}

	if (PreferredSource == ETargetSource::StandalonePreview && bPlaying && PreviewMeshComponent &&
		std::isfinite(DeltaTime) && DeltaTime > 0.0f)
	{
		StandaloneAnimInst->SetMode(ActiveTab == EEditorTab::BlendSpace1D
			? UBlendDemoAnimInstance::EMode::BlendSpace1D
			: UBlendDemoAnimInstance::EMode::TwoWayBlend);
		StandaloneAnimInst->BlendSpacePlayer.PlayRate = PreviewPlayRate;
		StandaloneAnimInst->TwoWayPlayerA.SetPlayRate(PreviewPlayRate);
		StandaloneAnimInst->TwoWayBlendSpaceA.PlayRate = PreviewPlayRate;
		StandaloneAnimInst->TwoWayPlayerB.SetPlayRate(PreviewPlayRate);
		PreviewMeshComponent->TickComponent(DeltaTime);
	}

	if (PreviewWorld)
	{
		PreviewWorld->GetScene().UpdateAllTransforms();
	}
}

void FAnimationBlendEditorPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(1100.0f, 900.0f), ImGuiCond_FirstUseEver);
	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
		ImGui::SetNextWindowCollapsed(false);
		bRequestFocus = false;
	}

	bool bPanelOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bPanelOpen))
	{
		const FBlendTarget Target = ResolveTarget();
		DrawTargetToolbar(Target);
		ImGui::Separator();

		if (ImGui::BeginTabBar("##BlendEditorTabs"))
		{
			const ImGuiTabItemFlags BSFlags = (bPendingTabSwitch && RequestedTab == EEditorTab::BlendSpace1D)
				? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			if (ImGui::BeginTabItem("BlendSpace 1D", nullptr, BSFlags))
			{
				ActiveTab = EEditorTab::BlendSpace1D;
				DrawBlendSpaceTab(Target);
				ImGui::EndTabItem();
			}

			const ImGuiTabItemFlags TWFlags = (bPendingTabSwitch && RequestedTab == EEditorTab::TwoWayBlend)
				? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			if (ImGui::BeginTabItem("Two-Way Blend", nullptr, TWFlags))
			{
				ActiveTab = EEditorTab::TwoWayBlend;
				DrawTwoWayBlendTab(Target);
				ImGui::EndTabItem();
			}

			bPendingTabSwitch = false;
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
	SetOpen(bPanelOpen);
}

void FAnimationBlendEditorPanel::DrawTargetToolbar(const FBlendTarget& Target)
{
	ImGui::TextUnformatted("Target:");
	ImGui::SameLine();
	if (ImGui::RadioButton("Standalone Preview", PreferredSource == ETargetSource::StandalonePreview))
	{
		PreferredSource = ETargetSource::StandalonePreview;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Scene / PIE Actor", PreferredSource == ETargetSource::SceneOrPIEActor))
	{
		PreferredSource = ETargetSource::SceneOrPIEActor;
	}
	ImGui::SameLine(0.0f, 12.0f);
	ImGui::TextColored(Target.bIsSceneTarget ? ImVec4(0.4f, 0.9f, 0.55f, 1.0f) : ImVec4(0.6f, 0.78f, 1.0f, 1.0f),
		"[%s]", Target.Label.c_str());

	if (!Target.bIsSceneTarget)
	{
		ImGui::Checkbox("Grid", &bRenderGrid);
		ImGui::SameLine();
		if (ImGui::Button(bPlaying ? "Pause" : "Play", ImVec2(58.0f, 0.0f)))
		{
			bPlaying = !bPlaying;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		ImGui::SliderFloat("PlayRate", &PreviewPlayRate, 0.1f, 2.5f, "%.2fx");
	}
}

void FAnimationBlendEditorPanel::DrawPreviewViewport(float ReservedBottomHeight)
{
	const float AvailY = std::max(1.0f, ImGui::GetContentRegionAvail().y);
	const float MinViewportHeight = std::min(320.0f, AvailY * 0.6f);
	const float ViewportHeight = std::max(MinViewportHeight, AvailY - ReservedBottomHeight);
	if (ImGui::BeginChild("##BlendPreviewViewportChild", ImVec2(0.0f, ViewportHeight), ImGuiChildFlags_Borders,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		const ImVec2 Avail = ImGui::GetContentRegionAvail();
		const uint32 Width = static_cast<uint32>(std::max(1.0f, Avail.x));
		const uint32 Height = static_cast<uint32>(std::max(1.0f, Avail.y));
		if (Width != ViewportClient.GetWidth() || Height != ViewportClient.GetHeight())
		{
			ViewportClient.Resize(Width, Height);
		}
		if (ViewportClient.GetColorTarget() && ViewportClient.GetColorTarget()->GetSRV())
		{
			const ImVec2 Min = ImGui::GetCursorScreenPos();
			const ImVec2 Size(static_cast<float>(Width), static_cast<float>(Height));
			ImGui::InvisibleButton("##BlendViewportImg", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
			ImGui::GetWindowDrawList()->AddImage(ViewportClient.GetColorTarget()->GetSRV(), Min, ImVec2(Min.x + Size.x, Min.y + Size.y));
			TickCameraInput();
		}
	}
	ImGui::EndChild();
}

void FAnimationBlendEditorPanel::TickCameraInput()
{
	if (ImGui::IsItemActivated())
	{
		ImGui::SetWindowFocus();
	}
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bCaptured = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Right);
	const float CamSpeedMul = Settings ? Settings->CameraSpeed : 1.0f;
	const float MouseSensMul = Settings ? Settings->MouseSensitivity : 1.0f;

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
	ViewportClient.ApplyCameraInput(Input, IO.DeltaTime, CameraSpeed * CamSpeedMul, 0.2f * MouseSensMul);
}

void FAnimationBlendEditorPanel::DrawBlendSpaceTab(const FBlendTarget& Target)
{
	if (!Target.bIsSceneTarget)
	{
		const ImGuiStyle& Style = ImGui::GetStyle();
		const float LineH = ImGui::GetTextLineHeightWithSpacing();
		const float FrameH = ImGui::GetFrameHeightWithSpacing();
		const float CanvasSectionH = LineH + 112.0f + FrameH + Style.ItemSpacing.y * 4.0f;
		const int32 SampleCount = (Target.BlendSpacePlayer && Target.BlendSpacePlayer->BlendSpace)
			? Target.BlendSpacePlayer->BlendSpace->SampleData.Num() : 4;
		const int32 VisibleSamples = std::clamp(SampleCount, 2, 6);
		const float DetailsSectionH = LineH + 10.0f + FrameH * static_cast<float>(VisibleSamples + 1) + Style.ItemSpacing.y * 4.0f + 12.0f;
		DrawPreviewViewport(CanvasSectionH + DetailsSectionH);
		ImGui::Spacing();
	}

	if (!Target.BlendSpacePlayer || !Target.SpeedPtr)
	{
		ImGui::TextDisabled("Selected actor does not currently have an active BlendSpacePlayer.");
		ImGui::TextDisabled("Select an actor using BlendSpace (or switch to Standalone Preview).");
		return;
	}

	if (ImGui::BeginChild("##BlendSpaceControls", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None))
	{
		DrawBlendSpaceCanvas(*Target.BlendSpacePlayer, Target.SpeedPtr);
		ImGui::Spacing();
		DrawBlendSpaceDetails(*Target.BlendSpacePlayer, Target.SpeedPtr, Target.CharacterInst);
	}
	ImGui::EndChild();
}

void FAnimationBlendEditorPanel::DrawBlendSpaceCanvas(FAnimNode_BlendSpacePlayer& BSPlayer, float* SpeedPtr)
{
	UBlendSpace* BS = BSPlayer.BlendSpace.get();
	const float MinX = (BS && BS->SampleData.Num() > 0) ? BS->SampleData[0].SamepleVector.X : 0.0f;
	const float MaxSampleX = (BS && BS->SampleData.Num() > 0) ? BS->SampleData[BS->SampleData.Num() - 1].SamepleVector.X : 600.0f;
	const float RangeX = std::max(MaxSampleX - MinX, 1.0f);

	ImGui::Text("1D BlendSpace Canvas (Drag green cursor for Speed, Drag blue dot to move sample)");

	const float CanvasWidth = std::max(ImGui::GetContentRegionAvail().x, 120.0f);
	const float CanvasHeight = 112.0f;
	const ImVec2 CanvasSize(CanvasWidth, CanvasHeight);
	const ImVec2 P0 = ImGui::GetCursorScreenPos();
	const ImVec2 P1(P0.x + CanvasSize.x, P0.y + CanvasSize.y);
	const float PaddingX = 28.0f;
	const float UsableWidth = std::max(CanvasSize.x - PaddingX * 2.0f, 10.0f);
	const float RailY = P0.y + CanvasHeight * 0.54f;

	ImDrawList* Draw = ImGui::GetWindowDrawList();
	ImGui::InvisibleButton("##BSEditorCanvas", CanvasSize, ImGuiButtonFlags_MouseButtonLeft);
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	const ImVec2 MousePos = ImGui::GetIO().MousePos;

	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && BS)
	{
		DraggingSampleIndex = INDEX_NONE;
		bDraggingCursor = true;
		for (int32 i = BS->SampleData.Num() - 1; i >= 0; --i)
		{
			const float Ratio = (BS->SampleData[i].SamepleVector.X - MinX) / RangeX;
			const float SampleX = P0.x + PaddingX + Ratio * UsableWidth;
			const float Dx = MousePos.x - SampleX;
			const float Dy = MousePos.y - RailY;
			if (Dx * Dx + Dy * Dy <= 100.0f)
			{
				SelectedSampleIndex = i;
				DraggingSampleIndex = i;
				bDraggingCursor = false;
				break;
			}
		}
	}

	if (bActive && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const float ClampedX = std::clamp(MousePos.x, P0.x + PaddingX, P0.x + PaddingX + UsableWidth);
		const float Norm = (ClampedX - (P0.x + PaddingX)) / UsableWidth;
		const float MappedValue = MinX + Norm * RangeX;

		if (DraggingSampleIndex != INDEX_NONE && BS && BS->SampleData.IsValidIndex(DraggingSampleIndex))
		{
			const FBlendSample Tracked = BS->SampleData[DraggingSampleIndex];
			BS->SampleData[DraggingSampleIndex].SamepleVector.X = MappedValue;
			BS->SortSamples();
			for (int32 i = 0; i < BS->SampleData.Num(); ++i)
			{
				if (BS->SampleData[i].Animation == Tracked.Animation &&
					BS->SampleData[i].SamepleVector.X == MappedValue)
				{
					DraggingSampleIndex = i;
					SelectedSampleIndex = i;
					break;
				}
			}
		}
		else if (bDraggingCursor)
		{
			*SpeedPtr = MappedValue;
		}
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		DraggingSampleIndex = INDEX_NONE;
		bDraggingCursor = false;
	}

	Draw->AddRectFilled(P0, P1, IM_COL32(24, 24, 28, 255), 6.0f);
	Draw->AddRect(P0, P1, IM_COL32(65, 68, 78, 255), 6.0f);

	const int32 Divisions = 4;
	for (int32 i = 0; i <= Divisions; ++i)
	{
		const float Frac = static_cast<float>(i) / static_cast<float>(Divisions);
		const float Val = MinX + Frac * RangeX;
		const float LineX = P0.x + PaddingX + Frac * UsableWidth;
		Draw->AddLine(ImVec2(LineX, P0.y + 4.0f), ImVec2(LineX, P1.y - 4.0f), IM_COL32(45, 48, 56, 255), 1.0f);

		char Buf[32];
		std::snprintf(Buf, sizeof(Buf), "%.1f", Val);
		Draw->AddText(ImVec2(LineX - 10.0f, P1.y - 18.0f), IM_COL32(140, 145, 160, 255), Buf);
	}

	Draw->AddLine(ImVec2(P0.x + PaddingX, RailY), ImVec2(P0.x + PaddingX + UsableWidth, RailY), IM_COL32(85, 88, 102, 255), 2.0f);

	const auto& Cache = BSPlayer.BlendSampleDataCache;
	if (BS)
	{
		for (int32 i = 0; i < BS->SampleData.Num(); ++i)
		{
			const FBlendSample& Sample = BS->SampleData[i];
			const float SampleRatio = (Sample.SamepleVector.X - MinX) / RangeX;
			const float SampleX = P0.x + PaddingX + SampleRatio * UsableWidth;

			float Weight = 0.0f;
			for (const auto& Cached : Cache)
			{
				if (Cached.SampleDataIndex == i)
				{
					Weight = Cached.TotalWeight;
					break;
				}
			}

			if (Weight > 0.001f)
			{
				const float Radius = 6.0f + Weight * 16.0f;
				Draw->AddCircleFilled(ImVec2(SampleX, RailY), Radius, IM_COL32(40, 200, 100, static_cast<int>(Weight * 85)));
				Draw->AddCircle(ImVec2(SampleX, RailY), Radius, IM_COL32(60, 240, 120, static_cast<int>(Weight * 220)), 24, 2.0f);
			}

			const bool bSel = (i == SelectedSampleIndex);
			const ImU32 MarkerCol = bSel ? IM_COL32(255, 195, 50, 255) : IM_COL32(55, 135, 245, 255);
			Draw->AddCircleFilled(ImVec2(SampleX, RailY), 5.5f, MarkerCol);
			Draw->AddCircle(ImVec2(SampleX, RailY), 5.5f, IM_COL32(235, 240, 250, 255), 16, 1.5f);

			const FString ShortName = ExtractShortClipName(Sample.Animation);
			const ImVec2 TextSize = ImGui::CalcTextSize(ShortName.c_str());
			Draw->AddText(ImVec2(SampleX - TextSize.x * 0.5f, RailY - 24.0f),
				bSel ? IM_COL32(255, 230, 130, 255) : IM_COL32(215, 220, 230, 255), ShortName.c_str());

			const float Dx = MousePos.x - SampleX;
			const float Dy = MousePos.y - RailY;
			if (bHovered && Dx * Dx + Dy * Dy <= 120.0f)
			{
				ImGui::SetTooltip("%s\nX: %.2f | Length: %.3fs\nWeight: %.3f (%.1f%%)",
					ShortName.c_str(), Sample.SamepleVector.X, Sample.GetSamplePlayLength(), Weight, Weight * 100.0f);
			}
		}
	}

	const float ClampedSpeed = std::clamp(*SpeedPtr, MinX, MinX + RangeX);
	const float CursorRatio = (ClampedSpeed - MinX) / RangeX;
	const float CursorX = P0.x + PaddingX + CursorRatio * UsableWidth;
	Draw->AddLine(ImVec2(CursorX, P0.y + 4.0f), ImVec2(CursorX, P1.y - 4.0f), IM_COL32(50, 240, 95, 190), 1.5f);
	Draw->AddCircleFilled(ImVec2(CursorX, RailY), 7.0f, IM_COL32(45, 245, 85, 255));
	Draw->AddCircle(ImVec2(CursorX, RailY), 7.0f, IM_COL32(255, 255, 255, 255), 18, 2.0f);

	char SpeedBuf[32];
	std::snprintf(SpeedBuf, sizeof(SpeedBuf), "%.2f", ClampedSpeed);
	const ImVec2 BadgeSize = ImGui::CalcTextSize(SpeedBuf);
	Draw->AddRectFilled(ImVec2(CursorX - BadgeSize.x * 0.5f - 4.0f, P0.y + 5.0f),
		ImVec2(CursorX + BadgeSize.x * 0.5f + 4.0f, P0.y + 7.0f + BadgeSize.y),
		IM_COL32(18, 20, 24, 225), 3.0f);
	Draw->AddText(ImVec2(CursorX - BadgeSize.x * 0.5f, P0.y + 6.0f), IM_COL32(80, 255, 120, 255), SpeedBuf);

	ImGui::SetNextItemWidth(std::max(140.0f, CanvasWidth - 110.0f));
	ImGui::SliderFloat("Speed (X)", SpeedPtr, MinX, MinX + RangeX, "%.2f");
}

void FAnimationBlendEditorPanel::DrawBlendSpaceDetails(FAnimNode_BlendSpacePlayer& BSPlayer, float* SpeedPtr, UCharacterAnimInstance* CharInst)
{
	if (CharInst)
	{
		bool bOverride = CharInst->IsSpeedOverrideEnabled();
		if (ImGui::Checkbox("Override PIE Script Speed", &bOverride))
		{
			CharInst->SetSpeedOverride(bOverride, *SpeedPtr);
		}
		else if (bOverride)
		{
			CharInst->SetSpeedOverride(true, *SpeedPtr);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Locks BlendSpace X to UI slider during PIE)");
	}

	UBlendSpace* BS = BSPlayer.BlendSpace.get();
	if (!BS) return;

	if (ImGui::BeginTable("##BlendDetailsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Weights & Harmonic Time Sync");
		ImGui::Separator();

		float HarmonicLen = 0.0f;
		float ArithmeticLen = 0.0f;
		ComputeBlendSpaceLengths(BS, BSPlayer.BlendSampleDataCache, HarmonicLen, ArithmeticLen);
		ImGui::Text("Harmonic Blended Length : %.3f s", HarmonicLen);
		ImGui::TextDisabled("Arithmetic Avg Length   : %.3f s (Diff: %.3f s)",
			ArithmeticLen, std::fabs(ArithmeticLen - HarmonicLen));

		char TimeBuf[64];
		std::snprintf(TimeBuf, sizeof(TimeBuf), "Norm Time: %.2f", BSPlayer.InternalTimeAccumulator);
		ImGui::ProgressBar(BSPlayer.InternalTimeAccumulator, ImVec2(-1.0f, 14.0f), TimeBuf);

		ImGui::Spacing();
		for (const FBlendSampleData& Cached : BSPlayer.BlendSampleDataCache)
		{
			const FString ClipName = ExtractShortClipName(Cached.Animation);
			const float Len = BS->SampleData.IsValidIndex(Cached.SampleDataIndex)
				? BS->GetBlendSample(Cached.SampleDataIndex).GetSamplePlayLength() : 0.0f;
			char BarText[96];
			std::snprintf(BarText, sizeof(BarText), "%s: %.1f%% (%.2fs / %.2fs)",
				ClipName.c_str(), Cached.TotalWeight * 100.0f, Cached.Time, Len);
			ImGui::ProgressBar(Cached.TotalWeight, ImVec2(-1.0f, 16.0f), BarText);
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Samples Editor");
		if (StandaloneAnimInst && BS == StandaloneAnimInst->BlendSpacePlayer.BlendSpace.get())
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset Defaults"))
			{
				ResetDefaultBlendSpaceSamples();
			}
		}
		ImGui::Separator();

		int32 RemoveIdx = INDEX_NONE;
		for (int32 i = 0; i < BS->SampleData.Num(); ++i)
		{
			FBlendSample& Sample = BS->SampleData[i];
			ImGui::PushID(i);
			const FString ClipName = ExtractShortClipName(Sample.Animation);
			ImGui::SetNextItemWidth(75.0f);
			if (ImGui::DragFloat("##SampleX", &Sample.SamepleVector.X, 1.0f, 0.0f, 2000.0f, "%.1f"))
			{
				SelectedSampleIndex = i;
				const UAnimSequence* EditedAnim = Sample.Animation;
				const float EditedX = Sample.SamepleVector.X;
				BS->SortSamples();
				for (int32 k = 0; k < BS->SampleData.Num(); ++k)
				{
					if (BS->SampleData[k].Animation == EditedAnim && BS->SampleData[k].SamepleVector.X == EditedX)
					{
						SelectedSampleIndex = k;
						break;
					}
				}
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(BS->SampleData.Num() <= 1);
			if (ImGui::SmallButton("X"))
			{
				RemoveIdx = i;
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextUnformatted(ClipName.c_str());
			ImGui::PopID();
			if (RemoveIdx != INDEX_NONE) break;
		}
		if (RemoveIdx != INDEX_NONE)
		{
			BS->RemoveSample(RemoveIdx);
			if (SelectedSampleIndex == RemoveIdx) SelectedSampleIndex = INDEX_NONE;
		}

		if (AvailableClips.IsEmpty())
		{
			UAssetManager::Get().GetFbxAnimationAssets(DefaultUalMeshKey, AvailableClips);
		}
		if (!AvailableClips.IsEmpty())
		{
			ImGui::Separator();
			AddSampleClipIndex = std::clamp(AddSampleClipIndex, 0, AvailableClips.Num() - 1);
			const char* PreviewName = AvailableClips[AddSampleClipIndex].DisplayName.c_str();
			ImGui::SetNextItemWidth(150.0f);
			if (ImGui::BeginCombo("##AddSampleClip", PreviewName))
			{
				for (int32 i = 0; i < AvailableClips.Num(); ++i)
				{
					const bool bSelected = (i == AddSampleClipIndex);
					if (ImGui::Selectable(AvailableClips[i].DisplayName.c_str(), bSelected))
					{
						AddSampleClipIndex = i;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(65.0f);
			ImGui::InputFloat("##AddSampleX", &AddSampleX, 0.0f, 0.0f, "%.1f");
			ImGui::SameLine();
			if (ImGui::Button("+ Add"))
			{
				if (UAnimSequence* Clip = AvailableClips[AddSampleClipIndex].Sequence)
				{
					SelectedSampleIndex = BS->AddSample(Clip, FVector(AddSampleX, 0.0f, 0.0f));
				}
			}
		}

		ImGui::EndTable();
	}
}

void FAnimationBlendEditorPanel::DrawTwoWayClipSelector(const char* Label, int32& InOutClipIndex, FAnimNode_SequencePlayer& Player, UWorld* /*WorldPtr*/)
{
	if (AvailableClips.IsEmpty())
	{
		ImGui::Text("%s: (No clips)", Label);
		return;
	}

	InOutClipIndex = std::clamp(InOutClipIndex, 0, AvailableClips.Num() - 1);
	const char* PreviewName = AvailableClips[InOutClipIndex].DisplayName.c_str();
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo(Label, PreviewName))
	{
		for (int32 i = 0; i < AvailableClips.Num(); ++i)
		{
			const bool bSel = (i == InOutClipIndex);
			if (ImGui::Selectable(AvailableClips[i].DisplayName.c_str(), bSel))
			{
				InOutClipIndex = i;
				Player.SetSequence(AvailableClips[i].Sequence);
				Player.SetCurrentPlayTime(0.0f);
			}
		}
		ImGui::EndCombo();
	}
}

void FAnimationBlendEditorPanel::DrawTwoWayBlendTab(const FBlendTarget& Target)
{
	if (!Target.bIsSceneTarget)
	{
		const ImGuiStyle& Style = ImGui::GetStyle();
		const float FrameH = ImGui::GetFrameHeightWithSpacing();
		const float ReservedBottomHeight = FrameH * 3.0f + 42.0f + 18.0f + Style.ItemSpacing.y * 5.0f + 16.0f;
		DrawPreviewViewport(ReservedBottomHeight);
		ImGui::Spacing();
	}

	if (!Target.TwoWayBlend || !Target.TwoWayAlphaPtr)
	{
		ImGui::TextDisabled("Selected actor does not currently use FAnimNode_TwoWayBlend.");
		ImGui::TextDisabled("Assign 'Scripts/two_way_blend_demo.lua' and press Play, or use Standalone Preview.");
		return;
	}

	if (ImGui::BeginChild("##TwoWayBlendControls", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None))
	{
		FAnimNode_TwoWayBlend& BlendNode = *Target.TwoWayBlend;
		float& Alpha = *Target.TwoWayAlphaPtr;
		Alpha = std::clamp(Alpha, 0.0f, 1.0f);

		if (!Target.bIsSceneTarget && StandaloneAnimInst)
		{
			if (ImGui::Checkbox("Use Locomotion BlendSpace as Input A", &bUseBlendSpaceForA))
			{
				StandaloneAnimInst->SetUseBlendSpaceForA(bUseBlendSpaceForA);
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(bUseBlendSpaceForA);
			if (ImGui::Button("Swap A <-> B"))
			{
				std::swap(ClipAIndex, ClipBIndex);
				if (AvailableClips.IsValidIndex(ClipAIndex))
				{
					StandaloneAnimInst->TwoWayPlayerA.SetSequence(AvailableClips[ClipAIndex].Sequence);
					StandaloneAnimInst->TwoWayPlayerA.SetCurrentPlayTime(0.0f);
				}
				if (AvailableClips.IsValidIndex(ClipBIndex))
				{
					StandaloneAnimInst->TwoWayPlayerB.SetSequence(AvailableClips[ClipBIndex].Sequence);
					StandaloneAnimInst->TwoWayPlayerB.SetCurrentPlayTime(0.0f);
				}
			}
			ImGui::EndDisabled();

			if (bUseBlendSpaceForA)
			{
				const UBlendSpace* BS = StandaloneAnimInst->TwoWayBlendSpaceA.BlendSpace.get();
				const float MaxX = (BS && BS->SampleData.Num() > 0) ? BS->SampleData[BS->SampleData.Num() - 1].SamepleVector.X : 600.0f;
				ImGui::SetNextItemWidth(220.0f);
				ImGui::SliderFloat("Input A BlendSpace Speed", &StandaloneAnimInst->BlendSpaceSpeed, 0.0f, MaxX, "%.1f");
			}
			else
			{
				DrawTwoWayClipSelector("Input A Clip", ClipAIndex, StandaloneAnimInst->TwoWayPlayerA, PreviewWorld);
			}
			ImGui::SameLine();
			DrawTwoWayClipSelector("Input B Clip", ClipBIndex, StandaloneAnimInst->TwoWayPlayerB, PreviewWorld);
		}
		else if (Target.BlendSpacePlayer && Target.CharacterInst)
		{
			bool bOverride = Target.CharacterInst->IsSpeedOverrideEnabled();
			float SpeedVal = Target.BlendSpacePlayer->X;
			if (ImGui::Checkbox("Override Input A BlendSpace Speed", &bOverride))
			{
				Target.CharacterInst->SetSpeedOverride(bOverride, SpeedVal);
			}
			if (bOverride)
			{
				const UBlendSpace* BS = Target.BlendSpacePlayer->BlendSpace.get();
				const float MaxX = (BS && BS->SampleData.Num() > 0) ? BS->SampleData[BS->SampleData.Num() - 1].SamepleVector.X : 3.0f;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::SliderFloat("##TWInputASpeed", &SpeedVal, 0.0f, MaxX, "%.2f"))
				{
					Target.CharacterInst->SetSpeedOverride(true, SpeedVal);
				}
			}
		}

		ImGui::Separator();

		// 시각적 Alpha 혼합 바
		const float BarWidth = std::max(ImGui::GetContentRegionAvail().x, 150.0f);
		const float BarHeight = 42.0f;
		const ImVec2 P0 = ImGui::GetCursorScreenPos();
		const ImVec2 P1(P0.x + BarWidth, P0.y + BarHeight);
		const float Pad = 18.0f;
		const float Usable = std::max(BarWidth - Pad * 2.0f, 10.0f);
		const float CenterY = P0.y + BarHeight * 0.5f;

		ImDrawList* Draw = ImGui::GetWindowDrawList();
		ImGui::InvisibleButton("##TwoWayAlphaBar", ImVec2(BarWidth, BarHeight), ImGuiButtonFlags_MouseButtonLeft);
		if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			bAutoPingPongAlpha = false;
			const float MouseX = std::clamp(ImGui::GetIO().MousePos.x, P0.x + Pad, P0.x + Pad + Usable);
			Alpha = (MouseX - (P0.x + Pad)) / Usable;
		}

		Draw->AddRectFilled(P0, P1, IM_COL32(24, 26, 30, 255), 6.0f);
		Draw->AddRect(P0, P1, IM_COL32(65, 68, 78, 255), 6.0f);
		const float HandleX = P0.x + Pad + Alpha * Usable;
		Draw->AddRectFilled(ImVec2(P0.x + Pad, CenterY - 4.0f), ImVec2(HandleX, CenterY + 4.0f), IM_COL32(70, 145, 245, 220), 2.0f);
		Draw->AddRectFilled(ImVec2(HandleX, CenterY - 4.0f), ImVec2(P0.x + Pad + Usable, CenterY + 4.0f), IM_COL32(235, 130, 60, 220), 2.0f);
		Draw->AddCircleFilled(ImVec2(HandleX, CenterY), 8.0f, IM_COL32(255, 255, 255, 255));

		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::SliderFloat("Alpha (0 = A, 1 = B)", &Alpha, 0.0f, 1.0f, "%.3f"))
		{
			bAutoPingPongAlpha = false;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("0.0 (A)")) { Alpha = 0.0f; bAutoPingPongAlpha = false; }
		ImGui::SameLine();
		if (ImGui::SmallButton("0.5")) { Alpha = 0.5f; bAutoPingPongAlpha = false; }
		ImGui::SameLine();
		if (ImGui::SmallButton("1.0 (B)")) { Alpha = 1.0f; bAutoPingPongAlpha = false; }
		ImGui::SameLine();
		ImGui::Checkbox("Auto Ping-Pong", &bAutoPingPongAlpha);
		if (bAutoPingPongAlpha)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			ImGui::SliderFloat("##PingPongSpd", &PingPongSpeed, 0.1f, 2.5f, "%.1fx");
		}

		auto DescribeNode = [](const FAnimNode_Base* Node) -> FString
		{
			if (const auto* Seq = dynamic_cast<const FAnimNode_SequencePlayer*>(Node))
			{
				return ExtractShortClipName(Seq->GetSequence());
			}
			if (dynamic_cast<const FAnimNode_BlendSpacePlayer*>(Node))
			{
				return "Locomotion BlendSpace";
			}
			return "Pose Node";
		};

		const FString NameA = DescribeNode(BlendNode.A.LinkedNode);
		const FString NameB = DescribeNode(BlendNode.B.LinkedNode);

		if (ImGui::BeginTable("##TwoWayWeights", 2, ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableNextColumn();
			char BufA[96];
			std::snprintf(BufA, sizeof(BufA), "Input A (%s): %.1f%%", NameA.c_str(), (1.0f - Alpha) * 100.0f);
			ImGui::ProgressBar(1.0f - Alpha, ImVec2(-1.0f, 18.0f), BufA);

			ImGui::TableNextColumn();
			char BufB[96];
			std::snprintf(BufB, sizeof(BufB), "Input B (%s): %.1f%%", NameB.c_str(), Alpha * 100.0f);
			ImGui::ProgressBar(Alpha, ImVec2(-1.0f, 18.0f), BufB);
			ImGui::EndTable();
		}
	}
	ImGui::EndChild();
}

UWorld* FAnimationBlendEditorPanel::ReleasePreviewWorld()
{
	ViewportClient.SetWorld(nullptr);
	bPlaying = false;
	bAutoPingPongAlpha = false;
	DraggingSampleIndex = INDEX_NONE;
	bDraggingCursor = false;
	SelectedSampleIndex = INDEX_NONE;
	StandaloneAnimInst = nullptr;
	DefaultMesh = nullptr;
	AvailableClips.Reset();

	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetAnimInstance(nullptr);
		PreviewMeshComponent = nullptr;
	}

	UWorld* Released = PreviewWorld;
	PreviewWorld = nullptr;
	return Released;
}
