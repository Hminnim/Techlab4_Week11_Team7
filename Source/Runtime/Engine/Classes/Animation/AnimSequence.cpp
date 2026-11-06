#include "EnginePCH.h"
#include "AnimSequence.h"
#include "BonePose.h"

namespace
{
	void GetKeyIndicesFromTime(
		int32& OutKeyIndex1, int32& OutKeyIndex2, float& OutAlpha,
		const float Time, const int32 NumKeys, const float SequenceLength, float FramesPerSecond
	)
	{
		if (Time <= 0.0 || NumKeys == 1)
		{
			OutKeyIndex1 = 0;
			OutKeyIndex2 = 0;
			OutAlpha = 0.0f;
			return;
		}

		const int32 LastIndex = NumKeys - 1;
		if (Time >= SequenceLength)
		{
			OutKeyIndex1 = LastIndex;
			OutKeyIndex2 = 0;
			OutAlpha = 0.0f;
			return;
		}

		const float KeyPos = Time * FramesPerSecond;

		// 앞 장
		const int32 KeyIndex1 = FMath::Clamp<int32>((int32)KeyPos, 0, NumKeys - 1);

		const float Alpha = KeyPos - static_cast<float>(KeyIndex1);

		// 뒷 장
		int32 KeyIndex2 = KeyIndex1 + 1;

		// 뒷 장이 끝 장일 경우
		if (KeyIndex2 == NumKeys)
		{
			KeyIndex2 = KeyIndex1;
		}

		OutKeyIndex1 = KeyIndex1;
		OutKeyIndex2 = KeyIndex2;
		OutAlpha = Alpha;
	}

	FVector SampleVectorKeys(const TArray<FVector3f>& Keys, const int32 KeyIndex1, const int32 KeyIndex2, const float Alpha)
	{
		if (Keys.Num() == 1)
			return Keys[0];

		const int32 LastIndex = Keys.Num() - 1;
		const FVector& A = Keys[FMath::Min(KeyIndex1, LastIndex)];
		const FVector& B = Keys[FMath::Min(KeyIndex2, LastIndex)];
		return FMath::Lerp(A, B, Alpha);
	}

	FQuat SampleRotationKeys(const TArray<FQuat4f>& Keys, const int32 KeyIndex1, const int32 KeyIndex2, const float Alpha)
	{
		if (Keys.Num() == 1)
			return Keys[0];

		const int32 LastIndex = Keys.Num() - 1;
		const FQuat4f& A = Keys[FMath::Min(KeyIndex1, LastIndex)];
		const FQuat4f& B = Keys[FMath::Min(KeyIndex2, LastIndex)];
		return FMath::Slerp(A, B, Alpha);
	}
}

UAnimDataModel::~UAnimDataModel()
{
	if (UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(GetOuter()))
		AnimSequence->OnDataModelDestroyed(this);
}

void UAnimDataModel::SetFrameRateAndNumberOfFrames(const FFrameRate& InFrameRate, int32 InNumberOfFrames)
{
	FrameRate = InFrameRate;
	NumberOfFrames = InNumberOfFrames;
	NumberOfKeys = InNumberOfFrames + 1;
	PlayLength = static_cast<float>(FrameRate.AsInterval()) * NumberOfFrames;
}

UAnimDataModel* UAnimSequenceBase::CreateDataModel()
{
	if (!DataModel)
		DataModel = NewObject<UAnimDataModel>(this);

	return DataModel;
}

UAnimSequenceBase::~UAnimSequenceBase()
{
	UAnimDataModel* Model = DataModel;
	DataModel = nullptr;
	delete Model;
}

void UAnimSequenceBase::OnDataModelDestroyed(const UAnimDataModel* Model)
{
	if (DataModel == Model)
		DataModel = nullptr;
}

void UAnimSequence::GetAnimationPose(FCompactPose& OutPose, const FAnimExtractContext& ExtractionContext) const
{
	GetBonePose(OutPose, ExtractionContext);
}

void UAnimSequence::GetBonePose(FCompactPose& OutPose, const FAnimExtractContext& ExtractionContext) const
{
	if (!OutPose.IsValid())
		return;

	OutPose.ResetToRefPose();

	if (!DataModel || DataModel->GetNumberOfKeys() <= 0)
		return;

	int32 KeyIndex1;
	int32 KeyIndex2;
	float Alpha;
	GetKeyIndicesFromTime(
		KeyIndex1, KeyIndex2, Alpha, 
		ExtractionContext.CurrentTime, 
		DataModel->GetNumberOfKeys(), 
		DataModel->GetPlayLength(), 
		static_cast<float>(DataModel->GetFrameRate().AsDecimal())
	);

	const FReferenceSkeleton& RefSkeleton = *OutPose.GetRefSkeleton();

	for (const FBoneAnimationTrack& Track : DataModel->GetBoneAnimationTracks())
	{
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Track.Name);
		if (BoneIndex == INDEX_NONE)
			continue;

		const FRawAnimSequenceTrack& Keys = Track.InternalTrack;
		if (Keys.PosKeys.Num() == 0 || Keys.RotKeys.Num() == 0 || Keys.ScaleKeys.Num() == 0)
			continue;

		const FVector Translation = SampleVectorKeys(Keys.PosKeys, KeyIndex1, KeyIndex2, Alpha);
		const FQuat Rotation = SampleRotationKeys(Keys.RotKeys, KeyIndex1, KeyIndex2, Alpha);
		const FVector Scale = SampleVectorKeys(Keys.ScaleKeys, KeyIndex1, KeyIndex2, Alpha);

		OutPose[BoneIndex] = MakeBoneTransform(Translation, Rotation, Scale);
	}
}