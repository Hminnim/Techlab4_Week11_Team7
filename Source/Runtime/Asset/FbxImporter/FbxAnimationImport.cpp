#include "EnginePCH.h"
#include "FbxImporter.h"
#include "Engine/Classes/Animation/AnimTypes.h"
#include "Math/Transform.h"

#include <fbxsdk.h>
#include <cmath>
#include <format>
#include <utility>

namespace
{
	struct FFbxBoneNode
	{
		FbxNode* Node = nullptr;
		int32 ParentIndex = INDEX_NONE;
	};

	bool IsBoneNode(const FbxNode* Node)
	{
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();

		if (!Attribute)
			return false;

		const FbxNodeAttribute::EType AttributeType = Attribute->GetAttributeType();
		return (AttributeType == FbxNodeAttribute::eSkeleton || AttributeType == FbxNodeAttribute::eNull);
	}

	void CollectBoneNodes(FbxNode* Node, int32 ParentIndex, TArray<FFbxBoneNode>& OutBones)
	{
		int32 ChildParentIndex = ParentIndex;
		if (IsBoneNode(Node))
		{
			ChildParentIndex = OutBones.Num(); // 자신의 인덱스를 자식들에게 넘겨준다.
			OutBones.Add({ Node, ParentIndex });
		}
		for (int i = 0; i < Node->GetChildCount(); ++i)
		{
			CollectBoneNodes(Node->GetChild(i), ChildParentIndex, OutBones);
		}
	}
}

bool FFbxImporter::LoadAnimation(const FString& Path, FAnimSequenceImportData& OutData, FString& OutError)
{
	OutError.clear();

	// Manager가 함수 종료 시 자신과 소유한 SDK 객체를 정리한다.
	FbxAutoDestroyPtr<FbxManager> Manager(FbxManager::Create());
	if (!Manager)
	{
		OutError = "Failed to create FBX manager.";
		return false;
	}

	FbxScene* Scene = nullptr;
	if (!LoadFbxScene(*Manager.Get(), Path, Scene, OutError))
	{
		return false;
	}

	if (!ConvertAndTriangulateScene(*Manager.Get(), *Scene, OutError))
	{
		return false;
	}

	FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(0);
	if (!AnimStack)
	{
		OutError = "No FbxAnimStack found.";
		return false;
	}

	Scene->SetCurrentAnimationStack(AnimStack);
	
	const FbxGlobalSettings& Settings = Scene->GetGlobalSettings();
	const FbxTime::EMode TimeMode = Settings.GetTimeMode();

	if (TimeMode == FbxTime::EMode::eCustom)
	{
		OutError = "Custom FBX frame rates are not supported yet.";
		return false;
	}
	const double Fps = FbxTime::GetFrameRate(TimeMode);
	const double RoundedFps = std::round(Fps);

	if (RoundedFps <= 0.0 || std::fabs(Fps - RoundedFps) > 1e-6)
	{
		OutError = std::format("Only integer frame rates are supported (got {})", Fps);
		return false;
	}

	const FbxTimeSpan Span = AnimStack->GetLocalTimeSpan();
	const int NumFrames = (int)std::lround(Span.GetDuration().GetFrameCountPrecise(TimeMode));
	if (NumFrames <= 0)
	{
		OutError = "AnimStack has no frames.";
		return false;
	}

	TArray<FFbxBoneNode> Bones;
	CollectBoneNodes(Scene->GetRootNode(), INDEX_NONE, Bones);

	if (Bones.IsEmpty())
	{
		OutError = "No skeleton nodes found";
		return false;
	}

	FAnimSequenceImportData Imported;
	Imported.FrameRate = FFrameRate(static_cast<int32>(RoundedFps), 1);
	Imported.NumberOfFrames = NumFrames;
	Imported.Tracks.SetNum(Bones.Num());

	for (int BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		Imported.Tracks[BoneIndex].Name = FName(Bones[BoneIndex].Node->GetName());

	TArray<FbxAMatrix> Globals;
	Globals.SetNum(Bones.Num());
	for (int K = 0; K <= NumFrames; ++K)
	{
		FbxTime Offset;
		Offset.SetFrame(K, TimeMode);
		const FbxTime Time = Span.GetStart() + Offset;

		for (int BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		{
			const FFbxBoneNode& Bone = Bones[BoneIndex];

			Globals[BoneIndex] = Bone.Node->EvaluateGlobalTransform(Time);
			FbxAMatrix Local = Globals[BoneIndex];

			if (Bone.ParentIndex != INDEX_NONE)
			{
				Local = Globals[Bone.ParentIndex].Inverse() * Globals[BoneIndex];
			}

			FRawAnimSequenceTrack& InternalTrack = Imported.Tracks[BoneIndex].InternalTrack;
			FTransform Key;
			if (!ConvertFbxMatrixToTransform(Local, Key, OutError))
			{
				OutError = std::format("Bone '{}' frame {}: {}", Bone.Node->GetName(), K, OutError);
				return false;
			}
			InternalTrack.PosKeys.Add(Key.Location);
			InternalTrack.RotKeys.Add(Key.Rotation.Quaternion());
			InternalTrack.ScaleKeys.Add(Key.Scale);
		}
	}

	OutData = std::move(Imported);
	return true;
}