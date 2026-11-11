#include "EnginePCH.h"
#include "AnimSequence.h"
#include "BonePose.h"
#include "AnimationRuntime.h"

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

void UAnimSequenceBase::GetAnimNotifies(const float& StartTime, const float& DeltaTime, FAnimNotifyContext& NotifyContext) const
{
	if(Notifies.Num() == 0 || GetPlayLength() <= 0.0f)
	{
		return;
	}

	bool const bPlayingBackwards = (DeltaTime < 0.f);
	float PreviousPosition = StartTime;
	float CurrentPosition = StartTime;
	float DesiredDeltaMove = DeltaTime;
	const float PlayLength = GetPlayLength();

	int32 MaxLoopCount = 2;
	if(PlayLength > 0.0f && std::abs(DeltaTime) > PlayLength)
	{
		MaxLoopCount = FMath::Clamp(int32(DesiredDeltaMove / PlayLength) + 2, 2, 1000);
	}

	for(int32 i = 0; i < MaxLoopCount; ++i)
	{
		const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, PlayLength);

		GetAnimNotifiesFromDeltaPosition(PreviousPosition, CurrentPosition, NotifyContext);

		if(AdvanceType == ETypeAdvanceAnim::Finished && NotifyContext.bLooping)
		{
			const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
			DesiredDeltaMove -= ActualDeltaMove;
			PreviousPosition = bPlayingBackwards ? GetPlayLength() : 0.f;
			CurrentPosition = PreviousPosition;
		}
		else
		{
			break;
		}
	}
}

void UAnimSequenceBase::GetAnimNotifiesFromDeltaPosition(const float& PreviousPosition, const float& CurrentPosition, FAnimNotifyContext& NotifyContext) const
{
	if(Notifies.Num() == 0)
	{
		return;
	}

	bool const bPlayingBackwards = (CurrentPosition < PreviousPosition);

	if(bPlayingBackwards)
	{
		for(int32 NotifyIndex = 0; NotifyIndex < Notifies.Num(); ++NotifyIndex)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = Notifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.GetTriggerTime();
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			if((NotifyStartTime < PreviousPosition) && (NotifyEndTime >= CurrentPosition))
			{
				NotifyContext.ActiveNotifies.Emplace(&AnimNotifyEvent, this);

				const bool bHasFinished = CurrentPosition <= FMath::Max(NotifyStartTime, 0.0f);
				if(bHasFinished)
				{
					NotifyContext.ActiveNotifies.Top().bHasFinished = true;
				}
			}
		}
	}
	else
	{
		const float PlayLength = GetPlayLength();
		for(int32 NotifyIndex = 0; NotifyIndex < Notifies.Num(); ++NotifyIndex)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = Notifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.GetTriggerTime();
			float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();
			if (PlayLength > 0.0f)
			{
				NotifyEndTime = FMath::Min(NotifyEndTime, PlayLength);
			}

			if((NotifyStartTime <= CurrentPosition) && (NotifyEndTime > PreviousPosition))
			{
				NotifyContext.ActiveNotifies.Emplace(&AnimNotifyEvent, this);

				const bool bHasFinished = CurrentPosition >= NotifyEndTime;
				if(bHasFinished)
				{
					NotifyContext.ActiveNotifies.Top().bHasFinished = true;
				}
			}
		}
	}
}

void UAnimSequenceBase::TickAssetPlayer(float& InOutTime, float DeltaMove, bool bLooping, FAnimNotifyQueue* OutNotifyQueue) const
{
	const float PreviousTime = InOutTime;
	
	FAnimationRuntime::AdvanceTime(bLooping, DeltaMove, InOutTime, GetPlayLength());
	
	if(OutNotifyQueue)
	{
		FAnimNotifyContext NotifyContext;
		NotifyContext.bLooping = bLooping;
		GetAnimNotifies(PreviousTime, DeltaMove, NotifyContext);
		OutNotifyQueue->AddAnimNotifies(NotifyContext.ActiveNotifies, OutNotifyQueue->AnimNotifies);
	}
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
	const FBoneAnimationTrack* RootMotionTrack = bEnableRootMotion ? FindRootMotionTrack() : nullptr;
	const TArray<FMeshBoneInfo>& BoneInfos = RefSkeleton.RawRefBoneInfo;
	const TArray<FBoneAnimationTrack>& BoneAnimationTracks = DataModel->GetBoneAnimationTracks();
	const int32 TrackNum = BoneAnimationTracks.Num();
	const int32 BoneNum = BoneInfos.Num();

	if (TrackBoneIndexHints.Num() != TrackNum)
		TrackBoneIndexHints.Init(INDEX_NONE, TrackNum);

	for (int32 i = 0; i < TrackNum; ++i)
	{
		const FBoneAnimationTrack& Track = BoneAnimationTracks[i];
		int32& BoneIndex = TrackBoneIndexHints[i];

		if (BoneIndex < 0 || BoneIndex >= BoneNum
			|| BoneInfos[BoneIndex].Name != Track.Name)
		{
			BoneIndex = RefSkeleton.FindBoneIndex(Track.Name);

			if (BoneIndex == INDEX_NONE)
				continue;
		}

		const FRawAnimSequenceTrack& Keys = Track.InternalTrack;
		if (Keys.PosKeys.Num() == 0 || Keys.RotKeys.Num() == 0 || Keys.ScaleKeys.Num() == 0)
			continue;

		FVector Translation = SampleVectorKeys(Keys.PosKeys, KeyIndex1, KeyIndex2, Alpha);
		const FQuat Rotation = SampleRotationKeys(Keys.RotKeys, KeyIndex1, KeyIndex2, Alpha);
		const FVector Scale = SampleVectorKeys(Keys.ScaleKeys, KeyIndex1, KeyIndex2, Alpha);

		if (&Track == RootMotionTrack)
			Translation = RefSkeleton.RawRefBonePose[BoneIndex].Location;

		OutPose[BoneIndex] = MakeBoneTransform(Translation, Rotation, Scale);
	}
}

FTransform UAnimSequence::ExtractRootMotion(float StartTime, float DeltaTime, bool bAllowLooping) const
{
	FTransform RootMotion;

	const FBoneAnimationTrack* RootTrack = FindRootMotionTrack();
	const float PlayLength = GetPlayLength();

	if (!RootTrack
		|| PlayLength <= 0.0f
		|| RootTrack->InternalTrack.PosKeys.IsEmpty()
		|| DeltaTime == 0.0f)
	{
		return RootMotion;
	}
	
	float Time = FMath::Clamp(StartTime, 0.0f, PlayLength);

	// 루프 아닐 시 시작과 끝 사이의 차이만 본다. 끝을 넘어가는 시간은 버린다.
	if (!bAllowLooping)
	{
		const float EndTime = FMath::Clamp(Time + DeltaTime, 0.0f, PlayLength);
		RootMotion.Location = SampleTrackLocation(*RootTrack, EndTime) - SampleTrackLocation(*RootTrack, Time);
		return RootMotion;
	}

	// 루프일 때 - 끝에 닿으면 반대쪽으로 넘어가 남은 시간만큼 이어서 더한다.
	FVector Delta(0.0f, 0.0f, 0.0f);
	float Remaining = DeltaTime;
	while (Remaining > 0.0f)
	{
		if (Time >= PlayLength)
			Time = 0.0f;

		const float Step = FMath::Min(Remaining, PlayLength - Time);
		if (Step <= 0.0f)
			break;

		Delta += SampleTrackLocation(*RootTrack, Time + Step) - SampleTrackLocation(*RootTrack, Time);
		Time += Step;
		Remaining -= Step;
	}
	
	while (Remaining < 0.0f)
	{
		if (Time <= 0.0f)
			Time = PlayLength;

		const float Step = FMath::Max(Remaining, -Time);
		if (Step >= 0.0f)
			break;
		Delta += SampleTrackLocation(*RootTrack, Time + Step) - SampleTrackLocation(*RootTrack, Time);
		Time += Step;
		Remaining -= Step;
	}

	RootMotion.Location = Delta;
	return RootMotion;
}

const FBoneAnimationTrack* UAnimSequence::FindRootMotionTrack() const
{
	if (!DataModel)
		return nullptr;

	const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();

	if (Tracks.IsEmpty())
		return nullptr;

	static const FName RootName("root");
	for (const FBoneAnimationTrack& Track : Tracks)
	{
		if (Track.Name == RootName)
			return &Track;
	}

	return &Tracks[0];
}

FVector UAnimSequence::SampleTrackLocation(const FBoneAnimationTrack& Track, float Time) const
{
	int32 KeyIndex1 = 0;
	int32 KeyIndex2 = 0;
	float Alpha = 0.0f;
	GetKeyIndicesFromTime(
		KeyIndex1, KeyIndex2, Alpha, Time,
		DataModel->GetNumberOfKeys(),
		DataModel->GetPlayLength(),
		static_cast<float>(DataModel->GetFrameRate().AsDecimal())
	);
	return SampleVectorKeys(Track.InternalTrack.PosKeys, KeyIndex1, KeyIndex2, Alpha);
}