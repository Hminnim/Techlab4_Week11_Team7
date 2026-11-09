#include "EnginePCH.h"
#include "Render/Renderer.h"
#include "Render/RenderCommand.h"
#include <algorithm>
#include <chrono>
#include <unordered_map>

#include "Render/SkyboxRenderer.h"
#include "Render/PostProcess/HeightFogRenderer.h"
#include "Render/PostProcess/FXAARenderer.h"
#include "Render/PostProcess/ToneMapData.h"
#include "Render/PostProcess/CameraOverlayRenderer.h"
#include "Text/TextRenderer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/StatOverlay.h"

namespace
{
	constexpr uint32 PerObjectSlotConstants = 16;
	constexpr uint32 MinPerObjectSlots = 1024;
}

FRenderer::FRenderer() = default;
FRenderer::~FRenderer() = default;

bool FRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	ObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	MaterialCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardMaterialConstants));
	DecalCB = RenderCommand::CreateConstantBuffer(sizeof(FDecalConstants));
	if (!ObjectCB || !ViewCB || !MaterialCB) return false;
	// 스키닝 유무 두 상수만 재사용한다. 매 Draw마다 Map하지 않는다.
	for (uint32 Index = 0; Index < 2; ++Index)
	{
		SkinningCB[Index] = RenderCommand::CreateConstantBuffer(16);
		if (!SkinningCB[Index] || !SkinningCB[Index]->GetBuffer()) return false;
		void* Mapped = RenderCommand::MapWriteDiscard(SkinningCB[Index].get());
		if (!Mapped) return false;
		const uint32 Constants[4] = { Index, 0, 0, 0 };
		std::memcpy(Mapped, Constants, sizeof(Constants));
		RenderCommand::Unmap(SkinningCB[Index].get());
	}
	GPUOcclusion.Init();
	return true;
}

void FRenderer::UploadViewMatrix(const FMatrix& ViewProjection)
{
	RenderCommand::UpdateBufferData(ViewCB.get(), &ViewProjection, sizeof(ViewProjection));
}

void FRenderer::BindSkinningBuffers(const FStructuredBuffer* SkinWeightBuffer, const FStructuredBuffer* SkinningMatrixBuffer)
{
	const uint32 Index = SkinWeightBuffer && SkinningMatrixBuffer ? 1u : 0u;
	RenderCommand::BindConstantBuffer(7, SkinningCB[Index].get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindShaderResource(14, SkinWeightBuffer, EShaderBindFlagBits::Vertex);
	RenderCommand::BindShaderResource(15, SkinningMatrixBuffer, EShaderBindFlagBits::Vertex);
}

void FRenderer::BindBatch(const FMeshDrawBatch& Batch)
{
	RenderCommand::BindPipelineState(Batch.Pipeline);
	if (Batch.bForceWireframe)
		RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
	
	RenderCommand::BindVertexBuffer(Batch.VertexBuffer);
	RenderCommand::BindIndexBuffer(Batch.IndexBuffer);
	
	if (Batch.bBindViewConstants)
		RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);
	for (uint32 Index = 0; Index < Batch.Textures.Num(); ++Index)
		RenderCommand::BindShaderResource(Index, Batch.Textures[Index].Texture,
			Batch.Textures[Index].Stages);
	
	RenderCommand::BindSamplerState(0, Batch.Sampler, Batch.SamplerStages);
	
	if (Batch.MaterialDataSize)
	{
		RenderCommand::UpdateBufferData(MaterialCB.get(), Batch.MaterialData.data(), Batch.MaterialDataSize);
		RenderCommand::BindConstantBuffer(Batch.MaterialSlot, MaterialCB.get(), Batch.MaterialStages);
	}
	// StaticMesh/CPU Draw에서도 명시적으로 해제해 직전 GPU 포즈가 새지 않게 한다.
	BindSkinningBuffers(Batch.SkinWeightBuffer, Batch.SkinningMatrixBuffer);
}

void FRenderer::BindObject(const FMeshDrawBatch& Batch, const FMeshDrawBatch::FItem& Item)
{
	if (Batch.bUseObjectSlots && bUsePerObjectSlots && PerObjectSlotCB &&
		Item.ObjectSlot != InvalidObjectSlot)
	{
		RenderCommand::BindConstantBufferRange(Batch.ObjectBindingSlot, PerObjectSlotCB.get(),
			Item.ObjectSlot * PerObjectSlotConstants, PerObjectSlotConstants, Batch.ObjectStages);
	}
	else
	{
		RenderCommand::UpdateBufferData(ObjectCB.get(), Item.ObjectData.data(), Item.ObjectDataSize);
		RenderCommand::BindConstantBuffer(Batch.ObjectBindingSlot, ObjectCB.get(), Batch.ObjectStages);
	}
}

void FRenderer::DrawMeshes(std::span<const FMeshDrawBatch> Batches)
{
	uint32 PreviousTextureCount = 0;
	for (const FMeshDrawBatch& Batch : Batches)
	{
		BindBatch(Batch);
		if (PreviousTextureCount > Batch.Textures.Num())
			RenderCommand::UnbindShaderResources(static_cast<uint32>(Batch.Textures.Num()),
				PreviousTextureCount - static_cast<uint32>(Batch.Textures.Num()),
				EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		
		PreviousTextureCount = static_cast<uint32>(Batch.Textures.Num());
		// CPU/GPU 모드 모두 SkeletalMesh 드로우만 측정한다. PS/래스터 비용도 포함한다.
		const uint32 QueryIndex = Batch.bSkeletalMesh ? BeginSkinningDrawProfile() : ~0u;
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			BindObject(Batch, Item);
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
		}
		EndSkinningDrawProfile(QueryIndex);
	}
}

void FRenderer::BeginSkinningProfileFrame()
{
	ActiveSkinningFrameQuery = INDEX_NONE;
	const uint32 Revision = USkeletalMeshComponent::GetSkinningModeRevision();
	if (SkinningProfileRevision != Revision)
	{
		SkinningDrawGPUStats = {};
		SkinningProfileRevision = Revision;
	}
	if (!FStatOverlay::IsEnabled(EStatFlags::Profile)) return;
	auto* Device = FRenderDevice::Get()->GetDevice();
	auto* Context = FRenderDevice::Get()->GetContext();
	FSkinningFrameQuery& Frame = SkinningFrameQueries[NextSkinningFrameQuery];
	if (Frame.bPending)
	{
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT Timing{};
		constexpr UINT Flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;
		HRESULT Result = Context->GetData(Frame.Disjoint.Get(), &Timing, sizeof(Timing), Flags);
		if (Result == S_FALSE) return;
		bool bValid = Result == S_OK && Frame.bValid && !Timing.Disjoint && Timing.Frequency != 0;
		uint64 TotalTicks = 0;
		for (uint32 Index = 0; Index < Frame.NumDraws && bValid; ++Index)
		{
			UINT64 Start = 0, End = 0;
			const HRESULT StartResult = Context->GetData(Frame.Draws[Index].Start.Get(), &Start, sizeof(Start), Flags);
			const HRESULT EndResult = Context->GetData(Frame.Draws[Index].End.Get(), &End, sizeof(End), Flags);
			if (StartResult == S_FALSE || EndResult == S_FALSE) return;
			bValid = StartResult == S_OK && EndResult == S_OK && End >= Start;
			if (bValid) TotalTicks += End - Start;
		}
		if (bValid && Frame.ModeRevision == Revision)
		{
			const double Seconds = static_cast<double>(TotalTicks) / Timing.Frequency;
			// 기존 집계 자료형의 ms/평균 표시를 재사용한다. 측정 원천은 GPU Timestamp다.
			SkinningDrawGPUStats.PushFrame(FPlatformTime::SecondsToCycles64(Seconds));
		}
		Frame.bPending = false;
	}
	if (!Frame.Disjoint)
	{
		const D3D11_QUERY_DESC Desc{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
		if (FAILED(Device->CreateQuery(&Desc, Frame.Disjoint.GetAddressOf()))) return;
	}
	Frame.NumDraws = 0;
	Frame.ModeRevision = Revision;
	Frame.bValid = true;
	Context->Begin(Frame.Disjoint.Get());
	ActiveSkinningFrameQuery = static_cast<int32>(NextSkinningFrameQuery);
}

uint32 FRenderer::BeginSkinningDrawProfile()
{
	if (ActiveSkinningFrameQuery == INDEX_NONE) return ~0u;
	FSkinningFrameQuery& Frame = SkinningFrameQueries[ActiveSkinningFrameQuery];
	if (Frame.NumDraws == Frame.Draws.Num())
	{
		FSkinningDrawQuery Query;
		const D3D11_QUERY_DESC Desc{ D3D11_QUERY_TIMESTAMP, 0 };
		auto* Device = FRenderDevice::Get()->GetDevice();
		if (FAILED(Device->CreateQuery(&Desc, Query.Start.GetAddressOf())) ||
			FAILED(Device->CreateQuery(&Desc, Query.End.GetAddressOf())))
		{
			Frame.bValid = false;
			return ~0u;
		}
		Frame.Draws.Add(std::move(Query));
	}
	const uint32 QueryIndex = Frame.NumDraws++;
	FSkinningDrawQuery& Query = Frame.Draws[QueryIndex];
	FRenderDevice::Get()->GetContext()->End(Query.Start.Get());
	return QueryIndex;
}

void FRenderer::EndSkinningDrawProfile(uint32 QueryIndex)
{
	if (ActiveSkinningFrameQuery == INDEX_NONE || QueryIndex == ~0u) return;
	FSkinningFrameQuery& Frame = SkinningFrameQueries[ActiveSkinningFrameQuery];
	FRenderDevice::Get()->GetContext()->End(Frame.Draws[QueryIndex].End.Get());
}

void FRenderer::EndSkinningProfileFrame()
{
	if (ActiveSkinningFrameQuery == INDEX_NONE) return;
	FSkinningFrameQuery& Frame = SkinningFrameQueries[ActiveSkinningFrameQuery];
	FRenderDevice::Get()->GetContext()->End(Frame.Disjoint.Get());
	Frame.bPending = true;
	NextSkinningFrameQuery = (NextSkinningFrameQuery + 1) % 4;
	ActiveSkinningFrameQuery = INDEX_NONE;
}

void FRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
{
	if (SlotCount <= PerObjectSlotCapacity) return;
	uint32 NewCapacity = std::max(PerObjectSlotCapacity * 2, MinPerObjectSlots);
	while (NewCapacity < SlotCount) NewCapacity *= 2;
	PerObjectSlotCB = RenderCommand::CreateConstantBuffer(NewCapacity * ObjectSlotBytes);
	if (!PerObjectSlotCB || !PerObjectSlotCB->GetBuffer())
	{
		HTR_LOG(Warning, "Per-object constant buffer creation failed; using per-draw updates.");
		PerObjectSlotCB.reset();
		PerObjectSlotCapacity = 0;
		return;
	}
	PerObjectSlotCapacity = NewCapacity;
}

uint8* FRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	if (!bUsePerObjectSlots || MaxSlots == 0) return nullptr;
	EnsurePerObjectSlotCapacity(MaxSlots);
	if (!PerObjectSlotCB) { bUsePerObjectSlots = false; return nullptr; }
	uint8* Destination = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Destination) bUsePerObjectSlots = false;
	return Destination;
}

void FRenderer::EndObjectConstants()
{
	if (PerObjectSlotCB) RenderCommand::Unmap(PerObjectSlotCB.get());
}

void FRenderer::InitScenePasses(const FString& SkyPanoramaPath)
{
	if (!SkyPanoramaPath.empty())
	{
		Skybox = MakeUnique<FSkyboxRenderer>();
		if (!Skybox->Init(SkyPanoramaPath))
		{
			HTR_LOG(Warning, "Skybox panorama not loaded: {}", SkyPanoramaPath);
			Skybox.reset();
		}
	}
	HeightFog = MakeUnique<FHeightFogRenderer>();
	HeightFog->Init(this);
	FXAA = MakeUnique<FFXAARenderer>();
	FXAA->Init(this);
	CameraOverlay = MakeUnique<FCameraOverlayRenderer>();   // FXAA->Init(this); 아래
	CameraOverlay->Init(this);
	ToneMapConstants = RenderCommand::CreateConstantBuffer(sizeof(FToneMapData));
	SceneText = MakeUnique<FTextRenderer>();
	SceneText->Init();
}

FOcclusionMeasureResult FRenderer::MeasureDrawVisibility(std::span<const FMeshDrawBatch> Batches)
{
	FOcclusionMeasureResult Result;
	uint32 Count = 0;
	for (const FMeshDrawBatch& Batch : Batches) Count += static_cast<uint32>(Batch.Items.Num());
	if (!Count) return Result;
	ID3D11Device* Device = FRenderDevice::Get()->GetDevice();
	ID3D11DeviceContext* Context = FRenderDevice::Get()->GetContext();
	if (!Device || !Context) return Result;
	const auto StartTime = std::chrono::high_resolution_clock::now();
	if (!DepthLessEqualReadOnly)
	{
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(Device->CreateDepthStencilState(&Desc, DepthLessEqualReadOnly.GetAddressOf()))) return Result;
	}
	while (OcclusionQueries.Num() < Count)
	{
		const D3D11_QUERY_DESC Desc{ D3D11_QUERY_OCCLUSION, 0 };
		ComPtr<ID3D11Query> Query;
		if (FAILED(Device->CreateQuery(&Desc, Query.GetAddressOf()))) return Result;
		OcclusionQueries.Add(std::move(Query));
	}
	uint32 QueryIndex = 0;
	for (const FMeshDrawBatch& Batch : Batches)
	{
		BindBatch(Batch);
		RenderCommand::SetBlendState(EBlendState::NoColorWrite);
		Context->OMSetDepthStencilState(DepthLessEqualReadOnly.Get(), 0);
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			BindObject(Batch, Item);
			ID3D11Query* Query = OcclusionQueries[QueryIndex++].Get();
			Context->Begin(Query);
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
			Context->End(Query);
		}
	}
	std::unordered_map<const void*, bool> ObjectVisible;
	ObjectVisible.reserve(Count);
	QueryIndex = 0;
	for (const FMeshDrawBatch& Batch : Batches)
		for (const FMeshDrawBatch::FItem& Item : Batch.Items)
		{
			UINT64 Samples = 0;
			while (Context->GetData(OcclusionQueries[QueryIndex++].Get(), &Samples, sizeof(Samples), 0) == S_FALSE) {}
			const bool bVisible = Samples > 0;
			++Result.TotalDraws;
			Result.TotalTriangles += Item.IndexCount / 3;
			if (bVisible)
			{
				++Result.VisibleDraws;
				Result.VisibleTriangles += Item.IndexCount / 3;
			}
			if (Item.bOccludedByGpu)
			{
				++Result.GPUOccludedDraws;
				if (bVisible) ++Result.FalseCulls;
			}
			bool& bObjectVisible = ObjectVisible[Item.ObjectKey];
			bObjectVisible = bObjectVisible || bVisible;
		}
	Result.TotalObjects = static_cast<uint32>(ObjectVisible.size());
	for (const auto& [Key, bVisible] : ObjectVisible)
		Result.VisibleObjects += bVisible ? 1 : 0;
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	Result.ElapsedMs = std::chrono::duration<double, std::milli>(
		std::chrono::high_resolution_clock::now() - StartTime).count();
	Result.bValid = true;
	return Result;
}

