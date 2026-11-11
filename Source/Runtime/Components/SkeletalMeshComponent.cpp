#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSceneProxy.h"
#include "Collision/Ray.h"
#include "Engine/Classes/Animation/AnimInstance.h"
#include "Engine/Classes/Animation/AnimNodeBase.h"
#include "Core/Stats/EditorStats.h"
#include "GameFramework/Actor.h"

#include <utility>

void USkeletalMeshComponent::SetGPUSkinningEnabled(bool bEnabled)
{
	if (bUseGPUSkinning == bEnabled) return;
	bUseGPUSkinning = bEnabled;
	++SkinningModeRevision;
	// 피킹 등 다른 Stat의 누적치는 유지한다.
	FStatRegistry::Reset(EditorStats::STAT_SkinningCPUUpdate);
	FStatRegistry::Reset(EditorStats::STAT_SkinningGPUPrepare);
}

USkeletalMeshComponent::USkeletalMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::PostPhysics;
}

USkeletalMeshComponent::~USkeletalMeshComponent() = default;

void USkeletalMeshComponent::SetAnimInstance(TUniquePtr<UAnimInstance> InInstance)
{
	if (AnimInstance)
	{
		AnimInstance->EndActiveNotifyStates();
	}
	AnimInstance = std::move(InInstance);
	if (AnimInstance)
	{
		AnimInstance->SetOwningComponent(this);
	}
	ResetToReferencePose();
	InitializeAnimation();
}

void USkeletalMeshComponent::InitializeAnimation()
{
	bAnimationInitialized = false;
	if (!AnimInstance || !SkeletalMesh) return;

	AnimInstance->NativeInitializeAnimation();
	AnimInstance->ConsumeRootMotion();
	if (FAnimNode_Base* Root = AnimInstance->GetRootNode())
	{
		Root->Initialize(FAnimationInitializeContext(AnimInstance.get()));
		bAnimationInitialized = true;
	}
}

void USkeletalMeshComponent::ApplyRootMotion(const FRootMotionMovementParams& RootMotion)
{
	if (!RootMotion.bHasRootMotion)
		return;

	AActor* Owner = GetOwner();
	USceneComponent* RootComponent = Owner ? Owner->GetRootComponent() : nullptr;

	if (!RootComponent)
		return;

	const FVector WorldDelta = ConvertLocalRootMotionToWorld(RootMotion.GetRootMotionTransform().Location);
	RootComponent->SetWorldLocation(RootComponent->GetWorldLocation() + WorldDelta, false);
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (!AnimInstance || !SkeletalMesh || DeltaTime <= 0.0f) return;
	if (!bAnimationInitialized) InitializeAnimation();
	if (!bAnimationInitialized) return;

	AnimInstance->NativeUpdateAnimation(DeltaTime);
	FAnimNode_Base* Root = AnimInstance->GetRootNode();
	Root->Update(FAnimationUpdateContext(AnimInstance.get(), DeltaTime));
	AnimInstance->TriggerAnimNotifies(DeltaTime);
	ApplyRootMotion(AnimInstance->ConsumeRootMotion());

	FPoseContext Output;
	Output.Pose.SetRefSkeleton(&SkeletalMesh->GetRefSkeleton());
	Root->Evaluate(Output);
	for (int32 BoneIndex = 0; BoneIndex < Output.Pose.GetNumBones(); ++BoneIndex)
	{
		SetBoneLocalTransform(BoneIndex, Output.Pose[BoneIndex]);
	}
}

FVector USkeletalMeshComponent::ConvertLocalRootMotionToWorld(const FVector& LocalDelta) const
{
	return GetWorldMatrix().TransformVector(LocalDelta);
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	// 메시가 아직 없어도 Proxy를 유지해 이후 SetSkeletalMesh 호출을 반영한다.
	return new FSkeletalMeshSceneProxy(this);
}

// 이전 메시의 출력 버퍼를 비우고, 새 메시의 기준 자세로 본을 초기화한다.
void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	if (SkeletalMesh == InMesh) return;
	// 원본 검증은 Importer가 마쳤다. 공유 IB가 없는 미초기화 리소스만 거부한다.
	if (InMesh && !InMesh->GetIndexBuffer())
	{
		HTR_LOG(Error, "SetSkeletalMesh: '{}' is not initialized", InMesh->GetPath());
		return;
	}

	MeshObject.reset();
	SkeletalMesh = InMesh;
	InitializePose();
	InitializeAnimation();

	// 메시가 바뀌면 재질 슬롯도 달라지므로 이전 덮어쓰기를 비우고 렌더 상태를 갱신한다.
	ClearOverrideMaterials();
	OnTransformDirty();
}

// 현재 본 자세로 정점을 변형하고 GPU 버퍼에 반영하는 진입점이다.
// 컬링 전에 호출하며, 자세가 바뀌지 않았으면 이전 결과를 재사용한다.
bool USkeletalMeshComponent::UpdateSkinnedMesh()
{
	if (!SkeletalMesh) return false;
	FScopeCycleCounter Counter(bUseGPUSkinning
		? EditorStats::STAT_SkinningGPUPrepare : EditorStats::STAT_SkinningCPUUpdate);
	// 포즈가 정지해 있어도 전역 모드 변경을 먼저 반영한다.
	if (MeshObject && MeshObject->IsGPUSkinned() != bUseGPUSkinning)
	{
		MeshObject.reset();
		MarkPoseDirty();
	}
	if (!bPoseDirty && MeshObject) return true;
	if (!MeshObject)
	{
		if (bUseGPUSkinning)
			MeshObject = MakeUnique<FSkeletalMeshObjectGPUSkin>(*SkeletalMesh);
		else
			MeshObject = MakeUnique<FSkeletalMeshObjectCPUSkin>(SkeletalMesh->GetMeshData(), SkeletalMesh->GetIndexBuffer());
	}
	if (!MeshObject->UpdateDynamicData(GetBoneComponentMatrices(), &GetDirtySkinningBones())) return false;
	// 본 이동과 모드 변경 모두 Bounds 및 그림자 캐시를 갱신한다.
	OnTransformDirty();
	if (!MeshObject->UploadDynamicData()) return false;
	// 업로드까지 성공한 자세만 반영 완료로 처리한다.
	ClearPoseDirty();
	return true;
}

bool USkeletalMeshComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
{
	// 현재 포즈의 스키닝 결과를 준비한다.
	// 포즈가 같으면 기존 결과를 재사용한다.
	if (!SkeletalMesh || !UpdateSkinnedMesh() || !MeshObject)
		return false;

	// 두 데이터 모두 Component 공간 기준이다.
	const TArray<FVertex>& Vertices = MeshObject->GetSkinnedVertices();
	const TArray<uint32>& Indices = SkeletalMesh->GetMeshData().Indices;

	// OutT에는 다른 컴포넌트까지 포함한 현재 최단 거리가 들어온다.
	float NearestT = OutT;
	bool bHit = false;

	for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
	{
		const FVector& P0 = Vertices[Indices[Index]].Position;
		const FVector& P1 = Vertices[Indices[Index + 1]].Position;
		const FVector& P2 = Vertices[Indices[Index + 2]].Position;

		float TriangleT = 0.0f;

		if (RayIntersectsTriangle(LocalRay, P0, P1, P2, TriangleT) && TriangleT < NearestT)
		{
			NearestT = TriangleT;
			bHit = true;
		}
	}

	// 실패했을 때는 호출자가 전달한 거리를 유지한다.
	if (bHit)
		OutT = NearestT;

	return bHit;
}

const FSkeletalMeshData* USkeletalMeshComponent::GetSkinnedMeshData() const
{
	return SkeletalMesh ? &SkeletalMesh->GetMeshData() : nullptr;
}

int32 USkeletalMeshComponent::GetNumMaterials() const
{
	return SkeletalMesh ? SkeletalMesh->GetMeshData().MaterialSlots.Num() : 0;
}

FString USkeletalMeshComponent::GetMaterialSlotName(int32 SlotIndex) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetMeshData().MaterialSlots.IsValidIndex(SlotIndex))
		return FString();
	return SkeletalMesh->GetMeshData().MaterialSlots[SlotIndex].Name;
}

UMaterial* USkeletalMeshComponent::GetDefaultMaterial(int32 SlotIndex) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetMeshData().MaterialSlots.IsValidIndex(SlotIndex))
		return nullptr;
	return SkeletalMesh->GetMaterial(static_cast<uint32>(SlotIndex));
}

// 스키닝 결과가 있으면 변형된 경계를, 아직 준비 전이면 원본 메시의 경계를 반환한다.
FBox USkeletalMeshComponent::CalcLocalBounds() const
{
	if (MeshObject) return MeshObject->GetLocalBounds();
	return SkeletalMesh ? SkeletalMesh->GetMeshData().ReferenceBounds : FBox{};
}

void USkeletalMeshComponent::DestroyRenderState()
{
	// Proxy를 먼저 제거한 뒤 그것이 참조할 수 있는 버퍼를 해제한다.
	Super::DestroyRenderState();
	MeshObject.reset();
	MarkPoseDirty();
}

void USkeletalMeshComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);
	if (bIsLoading)
	{
		// 리플렉션 로드는 Setter를 거치지 않아 현재 자세를 별도로 초기화한다.
		MeshObject.reset();
		InitializePose();
		InitializeAnimation();
		MarkRenderStateDirty();
		OnTransformDirty();
	}
}

void USkeletalMeshComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);
	if (Record.GetUnderlyingArchive().IsLoading())
	{
		MeshObject.reset();
		InitializePose();
		InitializeAnimation();
		MarkRenderStateDirty();
		OnTransformDirty();
	}
}
