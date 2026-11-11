#pragma once

#include "Components/SkinnedMeshComponent.h"
#include "Render/SkeletalMesh.h"
#include "Render/SkeletalMeshObject.h"
#include "SkeletalMeshComponent.generated.h"

class UAnimInstance;
struct FRootMotionMovementParams;

UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()

public:
	USkeletalMeshComponent();
	~USkeletalMeshComponent() override;

	// 모든 월드/컴포넌트가 공유한다. 객체별 스키닝 선택 속성은 두지 않는다.
	static void SetGPUSkinningEnabled(bool bEnabled);
	static bool IsGPUSkinningEnabled() { return bUseGPUSkinning; }
	static uint32 GetSkinningModeRevision() { return SkinningModeRevision; }

	// 구성된 인스턴스의 소유권을 넘긴다. nullptr이면 재생을 해제하고 기준 포즈로 되돌린다.
	void SetAnimInstance(TUniquePtr<UAnimInstance> InInstance);
	UAnimInstance* GetAnimInstance() const { return AnimInstance.get(); }
	void TickComponent(float DeltaTime) override;

	// 컴포넌트 공간 루트 모션 이동량을 월드 공간 이동량으로 변환
	FVector ConvertLocalRootMotionToWorld(const FVector& LocalDelta) const;

	// 메시 교체 시 기준 포즈로 초기화한다. nullptr이면 리소스와 본 자세를 비운다.
	void SetSkeletalMesh(USkeletalMesh* InMesh);
	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
	const FSkeletalMeshObject* GetMeshObject() const { return MeshObject.get(); }

	// 컬링 전 렌더 준비 단계에서 호출한다. 자세가 같으면 기존 버퍼를 재사용한다.
	bool UpdateSkinnedMesh();
	bool LineTraceComponentLocal(const FRay& LocalRay, float& OutT) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	int32 GetNumMaterials() const override;
	FString GetMaterialSlotName(int32 SlotIndex) const override;
	UMaterial* GetDefaultMaterial(int32 SlotIndex) const override;
	FBox CalcLocalBounds() const override;

	using Super::Serialize;
	void Serialize(json& Handle, bool bIsLoading) override;
	void Serialize(FStructuredArchive::FRecord Record) override;

protected:
	const FSkeletalMeshData* GetSkinnedMeshData() const override;
	void DestroyRenderState() override;

private:
	void InitializeAnimation();
	void ApplyRootMotion(const FRootMotionMovementParams& RootMotion);

	// 리소스 변경은 자세 초기화를 위해 SetSkeletalMesh를 거친다.
	UPROPERTY(EditAnywhere)
	USkeletalMesh* SkeletalMesh = nullptr;
	TUniquePtr<FSkeletalMeshObject> MeshObject;
	TUniquePtr<UAnimInstance> AnimInstance;
	bool bAnimationInitialized = false;
	inline static bool bUseGPUSkinning = true;
	inline static uint32 SkinningModeRevision = 0;
};
