#include "EnginePCH.h"
#include "AnimSequenceTest.h"
#include "Engine/Classes/Animation/AnimSequence.h"
#include "Render/ReferenceSkeleton.h"
#include "Math/EngineMath.h"
#include "Engine/Classes/Animation/BonePose.h"

UAnimSequence* AnimTest::CreateTestAnimSequence(const FReferenceSkeleton& RefSkeleton)
{
	UAnimSequence* Sequence = NewObject<UAnimSequence>();
	UAnimDataModel* Model = Sequence->CreateDataModel();
	Model->SetFrameRateAndNumberOfFrames(FFrameRate(30, 1), 60);

	// TODO: RefSkeleton에서 upperarm_l을 찾아 회전 트랙을 채운다.
	int32 BoneIndex = RefSkeleton.FindBoneIndex(FName("upperarm_l"));

	if (BoneIndex == INDEX_NONE)
	{
		HTR_LOG(Warning, "upperarm_l bone index not found");
	}
	else
	{
		const FName& BoneName = RefSkeleton.RawRefBoneInfo[BoneIndex].Name;
		const FTransform& BonePose = RefSkeleton.RawRefBonePose[BoneIndex];
		FRawAnimSequenceTrack RawTrack;
		RawTrack.PosKeys.Add(BonePose.Location);
		RawTrack.ScaleKeys.Add(BonePose.Scale);

		int32 NumberOfKeys = Model->GetNumberOfKeys();
		int32 NumberOfFrames = Model->GetNumberOfFrames();
		RawTrack.RotKeys.SetNum(NumberOfKeys);
		const FQuat RefRotation = GetBoneRotation(BonePose);
		for (int k = 0; k < NumberOfKeys; ++k)
		{
			const float Phase = 2 * PI * static_cast<float>(k) / static_cast<float>(NumberOfFrames);
			float Radian = FMath::DegreesToRadians(45.0f * std::sin(Phase));
			RawTrack.RotKeys[k] = RefRotation * FQuat::MakeFromAxisAngle(FVector(1, 0, 0), Radian);
		}

		FBoneAnimationTrack Track;
		Track.Name = BoneName.ToString();
		Track.InternalTrack = RawTrack;

		Model->BoneAnimationTracks.Add(Track);
	}

	return Sequence;
}