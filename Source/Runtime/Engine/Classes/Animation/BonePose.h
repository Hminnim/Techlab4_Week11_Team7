#pragma once

#include "Render/ReferenceSkeleton.h"

// 쿼터니언 회전 + 위치 + 스케일 -> 포즈용 FTransform
inline FTransform MakeBoneTransform(const FVector& Translation, const FQuat& Rotation, const FVector& Scale)
{
	return FTransform(Rotation.Normalized().ToFRotator(), Translation, Scale);
}

// 포즈용 FTransform -> 회전 쿼터니언
inline FQuat GetBoneRotation(const FTransform& BoneTransform)
{
	return BoneTransform.Rotation.Quaternion();
}

struct FBoneTransform
{
	FVector Translation = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity();
	FVector Scale = FVector::OneVector;

	void NormalizeRotation()
	{
		Rotation = Rotation.Normalized();
	}

	FTransform ToTransform() const
	{
		return MakeBoneTransform(Translation, Rotation, Scale);
	}
};

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
	inline const FReferenceSkeleton* GetRefSkeleton() const { return RefSkeleton; }
	inline FTransform& operator[](const int32 BoneIndex) { return Bones[BoneIndex]; }
	inline const FTransform& operator[] (const int32 BoneIndex) const { return Bones[BoneIndex]; }

	void NormalizeRotations()
	{
		for(FTransform& Bone : this->Bones)
		{
			//TODO : Normalize
		}
	}

protected:
	TArray<FTransform> Bones;							// Bone별 로컬 트랜스폼
	const FReferenceSkeleton* RefSkeleton = nullptr;	// 해당 포즈의 스켈레톤 포인터
};