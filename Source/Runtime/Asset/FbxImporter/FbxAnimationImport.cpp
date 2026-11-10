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

	FString MakeAnimationName(const char* StackName)
	{
		const FString FullName = StackName ? StackName : "";
		const size_t Bar = FullName.find_last_of('|');
		return Bar == FString::npos ? FullName : FullName.substr(Bar + 1);
	}

	bool IsNearlyConstant(const TArray<FVector3f>& Keys, float MaxDiff)
	{
		const FVector3f& FirstKey = Keys[0];
		
		for (uint32 i = 1; i < Keys.Num(); ++i)
		{
			if ((Keys[i] - FirstKey).Size() > MaxDiff)
				return false;
		}

		return true;
	}

	bool IsNearlyConstant(const TArray<FQuat4f>& Keys, float MaxAngleRadians)
	{
		const FQuat4f& FirstKey = Keys[0];
		
		for (int32 i = 1; i < Keys.Num(); ++i)
		{
			const FQuat4f& Current = Keys[i];
			const float Sign = FirstKey.Dot(Current) < 0.0f ? -1.0f : 1.0f;
			const FQuat4f D = FirstKey - Current * Sign;
			const float HalfChord = std::sqrt(D.X * D.X + D.Y * D.Y + D.Z * D.Z + D.W * D.W) * 0.5f;
			const float DiffAngle = 4.0f * std::asin(std::min(HalfChord, 1.0f));

			if (DiffAngle > MaxAngleRadians)
				return false;
		}

		return true;
	}

	void CompressConstantKeys(FRawAnimSequenceTrack& Track)
	{
		constexpr float MaxPosScaleDiff = 1e-6f;
		constexpr float MaxRotDiff = 1e-6f;
		const TArray<FQuat4f>& RotKeys = Track.RotKeys;

		if (Track.PosKeys.Num() > 1 && IsNearlyConstant(Track.PosKeys, MaxPosScaleDiff))
			Track.PosKeys.SetNum(1);

		if (Track.ScaleKeys.Num() > 1 && IsNearlyConstant(Track.ScaleKeys, MaxPosScaleDiff))
			Track.ScaleKeys.SetNum(1);

		if (Track.RotKeys.Num() > 1 && IsNearlyConstant(Track.RotKeys, MaxRotDiff))
			Track.RotKeys.SetNum(1);
	}
}

bool FFbxImporter::LoadAnimations(const FString& Path, TArray<FAnimSequenceImportData>& OutAnimations, FString& OutError)
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
		return false;

	// 애니메이션이 없는 메시 FBX의 경우 바로 true 반환
	const int32 AnimCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	if (AnimCount <= 0)
	{
		OutAnimations.Empty();
		return true;
	}

	if (!ConvertScene(*Manager.Get(), *Scene, OutError))
		return false;

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

	TArray<FFbxBoneNode> Bones;
	CollectBoneNodes(Scene->GetRootNode(), INDEX_NONE, Bones);

	if (Bones.IsEmpty())
	{
		OutError = "No skeleton nodes found";
		return false;
	}

	TArray<FAnimSequenceImportData> ImportedAnimations;
	TArray<FbxAMatrix> Globals;
	Globals.SetNum(Bones.Num());

	for (int i = 0; i < AnimCount; ++i)
	{
		FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(i);

		if (!AnimStack)
		{
			HTR_LOG(Warning, "FbxAnimStack not found: index {}", i);
			continue;
		}

		Scene->SetCurrentAnimationStack(AnimStack);

		const FbxTimeSpan Span = AnimStack->GetLocalTimeSpan();
		const int NumFrames = (int)std::lround(Span.GetDuration().GetFrameCountPrecise(TimeMode));

		if (NumFrames <= 0)
			continue;

		FAnimSequenceImportData Imported;
		Imported.Name = MakeAnimationName(AnimStack->GetName());
		Imported.FrameRate = FFrameRate(static_cast<int32>(RoundedFps), 1);
		Imported.NumberOfFrames = NumFrames;
		Imported.Tracks.SetNum(Bones.Num());

		for (int BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
			Imported.Tracks[BoneIndex].Name = FName(Bones[BoneIndex].Node->GetName());

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

		for (FBoneAnimationTrack& Track : Imported.Tracks)
		{
			CompressConstantKeys(Track.InternalTrack);
		}

		ImportedAnimations.Add(std::move(Imported));
	}

	OutAnimations = std::move(ImportedAnimations);
	return true;
}