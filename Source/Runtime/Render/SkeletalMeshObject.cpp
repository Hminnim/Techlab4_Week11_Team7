#include "EnginePCH.h"
#include "SkeletalMeshObject.h"

#include "Core/Async/TaskPool.h"
#include "Render/Buffer.h"
#include "Render/RenderCommand.h"
#include "Render/SkeletalMesh.h"
#include <algorithm>
#include <cstring>

FSkeletalMeshObject::FSkeletalMeshObject(const FSkeletalMeshData& InMeshData, FIndexBuffer* InIndexBuffer)
	: MeshData(InMeshData), LocalBounds(InMeshData.ReferenceBounds),
	  IndexBuffer(InIndexBuffer), SortID(NextSortID++)
{
	SkinningMatrices.SetNum(MeshData.RefBasesInvMatrix.Num());
}

FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectCPUSkin(const FSkeletalMeshData& InMeshData, FIndexBuffer* InIndexBuffer)
	: FSkeletalMeshObject(InMeshData, InIndexBuffer), SkinnedVertices(InMeshData.ReferenceVertices)
{
	BoneVertices.SetNum(MeshData.RefBasesInvMatrix.Num());
	DirtyVertices.SetNum(MeshData.ReferenceVertices.Num());
	for (int32 VertexIndex = 0; VertexIndex < MeshData.SkinWeights.Num(); ++VertexIndex)
	{
		const FSkinWeightInfo& Influences = MeshData.SkinWeights[VertexIndex];
		for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
		{
			if (Influences.InfluenceWeights[InfluenceIndex] == 0.0f) continue;
			BoneVertices[Influences.InfluenceBones[InfluenceIndex]].Add(VertexIndex);
		}
	}
}

FSkeletalMeshObject::~FSkeletalMeshObject() = default;
FSkeletalMeshObjectCPUSkin::~FSkeletalMeshObjectCPUSkin() = default;

// 본 행렬 계산은 공유하고, 정점/Bounds 준비는 선택된 구현에 맡긴다.
bool FSkeletalMeshObject::UpdateDynamicData(const TArray<FMatrix>& BoneComponentMatrices, const TArray<uint8>* DirtyBones)
{
	if (BoneComponentMatrices.Num() != MeshData.RefBasesInvMatrix.Num()) return false;
	if (DirtyBones && DirtyBones->Num() != BoneComponentMatrices.Num()) return false;
	// 객체를 새로 만들면 Dirty 마스크와 관계없이 모든 본의 캐시가 필요하다.
	if (!bSkinningInitialized) DirtyBones = nullptr;
	BuildSkinningMatrices(BoneComponentMatrices, DirtyBones);
	UpdateSkinning(DirtyBones);
	bSkinningInitialized = true;
	return true;
}

void FSkeletalMeshObjectCPUSkin::UpdateSkinning(const TArray<uint8>* DirtyBones)
{
	SkinVerticesCPU(DirtyBones);
	UpdateLocalBounds();
	bVertexBufferDirty = true;
}

// 기준 자세의 정점을 바인드 당시 본 공간으로 옮긴 뒤, 현재 본 자세를 적용하는 행렬을 만든다.
// 계산 결과는 메시 기준이며, 컴포넌트의 월드 변환은 렌더러가 적용한다.
void FSkeletalMeshObject::BuildSkinningMatrices(const TArray<FMatrix>& BoneComponentMatrices, const TArray<uint8>* DirtyBones)
{
	for (int32 BoneIndex = 0; BoneIndex < MeshData.RefBasesInvMatrix.Num(); ++BoneIndex)
	{
		if (DirtyBones && !(*DirtyBones)[BoneIndex]) continue;
		SkinningMatrices[BoneIndex] = MeshData.RefBasesInvMatrix[BoneIndex]
			* BoneComponentMatrices[BoneIndex];
	}
}

// 변경 본들의 영향 정점 합집합만 계산한다. 정점마다 모든 본 Weight를 사용한다.
void FSkeletalMeshObjectCPUSkin::SkinVerticesCPU(const TArray<uint8>* DirtyBones)
{
	if (DirtyBones)
	{
		DirtyVertexIndices.Reset();
		for (int32 BoneIndex = 0; BoneIndex < DirtyBones->Num(); ++BoneIndex)
		{
			if (!(*DirtyBones)[BoneIndex]) continue;
			for (int32 VertexIndex : BoneVertices[BoneIndex])
			{
				if (DirtyVertices[VertexIndex]) continue;
				DirtyVertices[VertexIndex] = 1;
				DirtyVertexIndices.Add(VertexIndex);
			}
		}
	}

	const bool bUseDirtyIndices = DirtyBones != nullptr;
	LastSkinnedVertexCount = bUseDirtyIndices ? DirtyVertexIndices.Num() : MeshData.ReferenceVertices.Num();
	const uint32 VertexCount = static_cast<uint32>(LastSkinnedVertexCount);
	FTaskPool& Pool = FTaskPool::Get();
	const uint32 ChunkCount = std::clamp(VertexCount / 1024u, 1u, Pool.GetNumThreads() * 4u);
	// 정점 목록의 중복을 제거한 뒤 각 작업이 서로 다른 출력 정점만 쓴다.
	Pool.ParallelFor(VertexCount, ChunkCount, [this, bUseDirtyIndices](uint32 Begin, uint32 End, uint32)
	{
		for (uint32 Index = Begin; Index < End; ++Index)
		{
			const int32 VertexIndex = bUseDirtyIndices ? DirtyVertexIndices[Index] : static_cast<int32>(Index);
			SkinVertexCPU(VertexIndex, SkinnedVertices[VertexIndex]);
			if (bUseDirtyIndices) DirtyVertices[VertexIndex] = 0;
		}
	});
}

// 정점에 연결된 본의 변환을 Weight만큼 섞어 위치·노멀·탄젠트를 계산한다.
void FSkeletalMeshObject::SkinVertexCPU(int32 VertexIndex, FVertex& Dest) const
{
	const FVertex& Source = MeshData.ReferenceVertices[VertexIndex];
	const FSkinWeightInfo& Influences = MeshData.SkinWeights[VertexIndex];
	// FMatrix 기본 생성자는 영행렬이다. 각 본 변환의 가중합을 만든다.
	FMatrix BlendedMatrix;
	bool bHasInfluence = false;
	for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
	{
		const float Weight = Influences.InfluenceWeights[InfluenceIndex];
		if (Weight == 0.0f) continue;
		bHasInfluence = true;
		BlendedMatrix += SkinningMatrices[Influences.InfluenceBones[InfluenceIndex]] * Weight;
	}

	// 이전 출력이 아닌 원본에서 매번 계산한다. UV와 Color도 원본 그대로 유지한다.
	Dest = Source;
	// Importer는 영향이 없는 정점을 0 Weight로 남긴다. 원본 메시 공간에 유지한다.
	if (!bHasInfluence) return;
	Dest.Position = BlendedMatrix.TransformPosition(Source.Position);

	const FVector XAxis(BlendedMatrix[0][0], BlendedMatrix[0][1], BlendedMatrix[0][2]);
	const FVector YAxis(BlendedMatrix[1][0], BlendedMatrix[1][1], BlendedMatrix[1][2]);
	const FVector ZAxis(BlendedMatrix[2][0], BlendedMatrix[2][1], BlendedMatrix[2][2]);
	const FVector CrossYZ = FVector::Cross(YAxis, ZAxis);
	const float Handedness = XAxis.Dot(CrossYZ) < 0.0f ? -1.0f : 1.0f;

	// 비균일 스케일에서도 표면에 수직인 방향을 유지하도록 노멀을 역전치로 변환한다.
	// 외적을 이용하면 역행렬을 직접 구하지 않고 같은 방향을 얻을 수 있다.
	FVector Normal = (CrossYZ * Source.Normal.X
		+ FVector::Cross(ZAxis, XAxis) * Source.Normal.Y
		+ FVector::Cross(XAxis, YAxis) * Source.Normal.Z) * Handedness;
	Dest.Normal = Normal.GetSafeNormal(SMALL_NUMBER, Source.Normal.GetSafeNormal());

	const FVector SourceTangent(Source.Tangent.X, Source.Tangent.Y, Source.Tangent.Z);
	FVector Tangent = BlendedMatrix.TransformVector(SourceTangent);
	// 노멀 방향 성분을 제거해 탄젠트가 표면에 평행하도록 맞춘다.
	Tangent -= Dest.Normal * Tangent.Dot(Dest.Normal);
	// 본 변환을 섞으면서 탄젠트가 사라지면 노멀에 수직인 방향을 새로 잡는다.
	if (Tangent.SizeSquared() < SMALL_NUMBER)
	{
		const FVector Axis = std::abs(Dest.Normal.Z) < 0.9f ? FVector(0, 0, 1) : FVector(0, 1, 0);
		Tangent = FVector::Cross(Axis, Dest.Normal);
	}
	Dest.Tangent = FVector4(Tangent.GetSafeNormal(), Source.Tangent.W * Handedness);
}

// 변형된 정점을 모두 포함하는 경계 상자를 구해 컬링과 그림자 범위 계산에 사용한다.
void FSkeletalMeshObjectCPUSkin::UpdateLocalBounds()
{
	LocalBounds.Min = LocalBounds.Max = SkinnedVertices[0].Position;
	for (const FVertex& Vertex : SkinnedVertices)
	{
		LocalBounds.Min = FMath::Min(LocalBounds.Min, Vertex.Position);
		LocalBounds.Max = FMath::Max(LocalBounds.Max, Vertex.Position);
	}
}

// 첫 업로드에 필요한 동적 정점 버퍼를 한 번만 생성한다.
bool FSkeletalMeshObjectCPUSkin::InitializeResources()
{
	if (!IndexBuffer) return false;
	if (DynamicVertexBuffer) return true;
	// IB는 리소스의 공유 버퍼를 사용하고, 출력용 VB만 컴포넌트별로 생성한다.
	auto NewVertexBuffer = RenderCommand::CreateDynamicVertexBuffer(
		static_cast<uint32>(sizeof(FVertex) * SkinnedVertices.Num()), sizeof(FVertex));
	if (!NewVertexBuffer || !NewVertexBuffer->GetBuffer()) return false;
	DynamicVertexBuffer = std::move(NewVertexBuffer);
	return true;
}

// 변형한 정점 전체를 GPU에 옮기고, 성공하면 업로드 대기 상태를 해제한다.
bool FSkeletalMeshObjectCPUSkin::UploadDynamicData()
{
	if (!bVertexBufferDirty) return true;
	if (!InitializeResources()) return false;
	void* Mapped = RenderCommand::MapWriteDiscard(DynamicVertexBuffer.get());
	if (!Mapped) return false;
	std::memcpy(Mapped, SkinnedVertices.GetData(), sizeof(FVertex) * SkinnedVertices.Num());
	RenderCommand::Unmap(DynamicVertexBuffer.get());
	bVertexBufferDirty = false;
	return true;
}

FMeshGeometry FSkeletalMeshObjectCPUSkin::GetGeometry() const
{
	// 처음 업로드하기 전이나 갱신 실패 시에는 미완성 버퍼를 제출하지 않는다.
	return bVertexBufferDirty ? FMeshGeometry{}
		: FMeshGeometry{ DynamicVertexBuffer.get(), IndexBuffer, SortID, nullptr, nullptr, true };
}

FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectGPUSkin(USkeletalMesh& InMesh)
	: FSkeletalMeshObject(InMesh.GetMeshData(), InMesh.GetIndexBuffer()), Mesh(InMesh)
{
}

FSkeletalMeshObjectGPUSkin::~FSkeletalMeshObjectGPUSkin() = default;

void FSkeletalMeshObjectGPUSkin::UpdateSkinning(const TArray<uint8>*)
{
	// 비음수 정규화 Weight의 가중합은 변환된 상자들의 합집합 AABB 안에 있다.
	// GPU 모드의 컬링을 위해 정점 전체를 CPU에서 다시 스키닝하지 않는다.
	LocalBounds = MeshData.ReferenceBounds;
	for (const FMatrix& Matrix : SkinningMatrices)
		LocalBounds.Expand(MeshData.ReferenceBounds.GetWorldAABB(Matrix));
	PickingVertices.Reset();
	bSkinningBufferDirty = true;
}

bool FSkeletalMeshObjectGPUSkin::UploadDynamicData()
{
	if (!bSkinningBufferDirty) return true;
	if (!Mesh.InitializeGPUResources()) return false;
	if (!SkinningMatrixBuffer)
	{
		auto NewBuffer = RenderCommand::CreateDynamicStructuredBuffer(
			static_cast<uint32>(SkinningMatrices.Num()), sizeof(FMatrix));
		if (!NewBuffer || !NewBuffer->GetBuffer() || !NewBuffer->GetSRV()) return false;
		SkinningMatrixBuffer = std::move(NewBuffer);
	}
	void* Mapped = RenderCommand::MapWriteDiscard(SkinningMatrixBuffer.get());
	if (!Mapped) return false;
	std::memcpy(Mapped, SkinningMatrices.GetData(), sizeof(FMatrix) * SkinningMatrices.Num());
	RenderCommand::Unmap(SkinningMatrixBuffer.get());
	bSkinningBufferDirty = false;
	return true;
}

FMeshGeometry FSkeletalMeshObjectGPUSkin::GetGeometry() const
{
	if (bSkinningBufferDirty) return {};
	return { Mesh.GetReferenceVertexBuffer(), IndexBuffer, SortID,
		Mesh.GetSkinWeightBuffer(), SkinningMatrixBuffer.get(), true };
}

const TArray<FVertex>& FSkeletalMeshObjectGPUSkin::GetSkinnedVertices() const
{
	if (PickingVertices.IsEmpty())
	{
		PickingVertices.SetNum(MeshData.ReferenceVertices.Num());
		for (int32 Index = 0; Index < PickingVertices.Num(); ++Index)
			SkinVertexCPU(Index, PickingVertices[Index]);
	}
	return PickingVertices;
}
