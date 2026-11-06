#pragma once

struct FReferenceSkeleton;
class UAnimSequence;

// 테스트 전용 코드
namespace AnimTest
{
	UAnimSequence* CreateTestAnimSequence(const FReferenceSkeleton& RefSkeleton);
}