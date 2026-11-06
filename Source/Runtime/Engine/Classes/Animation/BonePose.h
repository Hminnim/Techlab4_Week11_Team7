#pragma once

#include "Render/ReferenceSkeleton.h"

struct FCompactPose
{
	// 스켈레톤 지정
	void SetRefSkeleton(const FReferenceSkeleton* InSkeleton)
	{
		RefSkeleton = InSkeleton;

		if (RefSkeleton)
			ResetToRefPose();
		else
			Bones.Reset();
	}

	void ResetToRefPose()
	{
		if (!RefSkeleton)
			return;

		Bones = RefSkeleton->RawRefBonePose;
	}

	inline bool IsValid() const { return RefSkeleton && Bones.Num() == RefSkeleton->RawRefBonePose.Num(); }
	inline int32 GetNumBones() const { return Bones.Num(); }
	inline FTransform& operator[](const int32 BoneIndex) { return Bones[BoneIndex]; }
	inline const FTransform& operator[] (const int32 BoneIndex) const { return Bones[BoneIndex]; }

protected:
	TArray<FTransform> Bones;							// Bone별 로컬 트랜스폼
	const FReferenceSkeleton* RefSkeleton = nullptr;	// 해당 포즈의 스켈레톤 포인터
};