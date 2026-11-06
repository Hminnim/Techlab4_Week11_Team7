#include "EnginePCH.h"
#include "AnimSequence.h"
#include "BonePose.h"

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
}