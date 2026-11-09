#pragma once

#include "Render/MeshGeometry.h"
#include "Render/SkeletalMeshData.h"

class FStructuredBuffer;
class USkeletalMesh;

// 공유 메시 리소스는 이 객체보다 오래 살아야 한다. 포즈별 본 행렬만 공통으로 관리한다.
class FSkeletalMeshObject
{
public:
	FSkeletalMeshObject(const FSkeletalMeshData& InMeshData, FIndexBuffer* InIndexBuffer);
	virtual ~FSkeletalMeshObject();

	// 본 행렬을 즉시 스키닝 행렬로 변환하므로 컴포넌트의 자세 배열을 보관하지 않는다.
	// DirtyBones는 자손까지 포함한 마스크. 생략하거나 첫 갱신이면 전체를 계산한다.
	bool UpdateDynamicData(const TArray<FMatrix>& BoneComponentMatrices, const TArray<uint8>* DirtyBones = nullptr);
	virtual bool UploadDynamicData() = 0;
	virtual FMeshGeometry GetGeometry() const = 0;
	virtual const TArray<FVertex>& GetSkinnedVertices() const = 0;
	virtual bool IsGPUSkinned() const { return false; }
	const FBox& GetLocalBounds() const { return LocalBounds; }

protected:
	virtual void UpdateSkinning(const TArray<uint8>* DirtyBones) = 0;
	// CPU 렌더링과 GPU 모드의 삼각형 피킹이 같은 수식을 사용한다.
	void SkinVertexCPU(int32 VertexIndex, FVertex& Dest) const;

	const FSkeletalMeshData& MeshData;
	TArray<FMatrix> SkinningMatrices;
	FBox LocalBounds;
	FIndexBuffer* IndexBuffer = nullptr;
	const uint16 SortID;

private:
	void BuildSkinningMatrices(const TArray<FMatrix>& BoneComponentMatrices, const TArray<uint8>* DirtyBones);
	bool bSkinningInitialized = false;
	static inline uint16 NextSortID = 0;
};

// 기존 CPU 정점 갱신, 변경 정점 최적화, 정확한 Bounds 계산을 유지한다.
class FSkeletalMeshObjectCPUSkin final : public FSkeletalMeshObject
{
public:
	FSkeletalMeshObjectCPUSkin(const FSkeletalMeshData& InMeshData, FIndexBuffer* InIndexBuffer);
	~FSkeletalMeshObjectCPUSkin() override;
	bool UploadDynamicData() override;
	FMeshGeometry GetGeometry() const override;
	const TArray<FVertex>& GetSkinnedVertices() const override { return SkinnedVertices; }
	int32 GetLastSkinnedVertexCount() const { return LastSkinnedVertexCount; }

private:
	void UpdateSkinning(const TArray<uint8>* DirtyBones) override;
	bool InitializeResources();
	void SkinVerticesCPU(const TArray<uint8>* DirtyBones);
	void UpdateLocalBounds();

	TArray<FVertex> SkinnedVertices;
	// 변경 본에서 정점으로 바로 접근하고, 여러 본에 걸친 정점은 한 번만 계산한다.
	TArray<TArray<int32>> BoneVertices;
	TArray<uint8> DirtyVertices;
	TArray<int32> DirtyVertexIndices;
	int32 LastSkinnedVertexCount = 0;
	TUniquePtr<FVertexBuffer> DynamicVertexBuffer;
	bool bVertexBufferDirty = true;
};

// 원본 VB/Weight는 메시가 공유하고, 현재 포즈의 행렬 버퍼만 객체별로 소유한다.
class FSkeletalMeshObjectGPUSkin final : public FSkeletalMeshObject
{
public:
	explicit FSkeletalMeshObjectGPUSkin(USkeletalMesh& InMesh);
	~FSkeletalMeshObjectGPUSkin() override;
	bool UploadDynamicData() override;
	FMeshGeometry GetGeometry() const override;
	const TArray<FVertex>& GetSkinnedVertices() const override;
	bool IsGPUSkinned() const override { return true; }

private:
	void UpdateSkinning(const TArray<uint8>* DirtyBones) override;
	USkeletalMesh& Mesh;
	TUniquePtr<FStructuredBuffer> SkinningMatrixBuffer;
	bool bSkinningBufferDirty = true;
	// 일반 렌더링에서는 비어 있다. 피킹 요청이 있을 때만 CPU 정점을 계산한다.
	mutable TArray<FVertex> PickingVertices;
};
