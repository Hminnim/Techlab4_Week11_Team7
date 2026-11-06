#include "EnginePCH.h"
#include "AnimSequenceTest.h"
#include "Engine/Classes/Animation/AnimSequence.h"

UAnimSequence* AnimTest::CreateTestAnimSequence(const FReferenceSkeleton& RefSkeleton)
{
	UAnimSequence* Sequence = NewObject<UAnimSequence>();
	UAnimDataModel* Model = Sequence->CreateDataModel();
	Model->SetFrameRateAndNumberOfFrames(FFrameRate(30, 1), 60);

	// TODO: RefSkeleton에서 upperarm_l을 찾아 회전 트랙을 채운다.
	(void)RefSkeleton;

	return Sequence;
}