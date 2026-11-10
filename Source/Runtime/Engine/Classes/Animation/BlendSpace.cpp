#include "EnginePCH.h"
#include "BlendSpace.h"
#include "BonePose.h"
#include "AnimationRuntime.h"

int32 UBlendSpace::AddSample(const FVector& SampleValue)
{
    FBlendSample SampleToAdd;
    SampleToAdd.Animation = nullptr;
    SampleToAdd.SamepleVector = SampleValue;

    SampleData.Add(SampleToAdd);

    std::sort(SampleData.begin(), SampleData.end(), [](const FBlendSample& A, const FBlendSample& B)
        {
            return A.SamepleVector.X < B.SamepleVector.X;
        });

    return SampleData.Num() - 1;
}

int32 UBlendSpace::AddSample(UAnimSequence* Animation, const FVector& SampleValue)
{
    FBlendSample SampleToAdd;
    SampleToAdd.Animation = Animation;
    SampleToAdd.SamepleVector = SampleValue;

    SampleData.Add(SampleToAdd);

    std::sort(SampleData.begin(), SampleData.end(), [](const FBlendSample& A, const FBlendSample& B)
        {
            return A.SamepleVector.X < B.SamepleVector.X;
        });

    return SampleData.Num() - 1;
}

bool UBlendSpace::GetSamplesFromBlendInput(const FVector& BlendInput, TArray<FBlendSampleData>& OutSampleDataList) const
{
    OutSampleDataList.Reset();

    const int32 SampleNum = SampleData.Num();
    if(SampleNum == 0)
    {
        return false;
    }

    if(SampleNum == 1)
    {
        FBlendSampleData NewSampleData;
        NewSampleData.TotalWeight = 1.0f;
        NewSampleData.Animation = SampleData[0].Animation;
        NewSampleData.SampleDataIndex = 0;
        OutSampleDataList.Add(NewSampleData);
        return true;
    }

    // Blend 1D only
    const float MinX = SampleData[0].SamepleVector.X;
    const float MaxX = SampleData[SampleNum - 1].SamepleVector.X;
    const float ClampedX = FMath::Clamp(BlendInput.X, MinX, MaxX);

    // Compare near index
    for(int32 index = 0; index < SampleNum - 1; ++index)
    {
        const float X0 = SampleData[index].SamepleVector.X;
        const float X1 = SampleData[index + 1].SamepleVector.X;

        if(ClampedX >= X0 && ClampedX <= X1)
        {
            const float Dist = X1 - X0;
            const float Alpha = (Dist > 0.0f) ? ((ClampedX - X0) / Dist) : 0.0f;

            if(1.0f - Alpha > 0.0f)
            {
                FBlendSampleData NewSampleData;
                NewSampleData.TotalWeight = 1.0f - Alpha;
                NewSampleData.Animation = SampleData[index].Animation;
                NewSampleData.SampleDataIndex = index;
                OutSampleDataList.Add(NewSampleData);
            }
            if(Alpha > 0.0f)
            {
                FBlendSampleData NewSampleData;
                NewSampleData.TotalWeight = Alpha;
                NewSampleData.Animation = SampleData[index + 1].Animation;
                NewSampleData.SampleDataIndex = index + 1;
                OutSampleDataList.Add(NewSampleData);
            }

            return true;
        }
    }

    return false;
}

float UBlendSpace::GetAnimationLengthFromSampleData(const TArray<FBlendSampleData>& SampleDataList) const
{
    float BlendedNormalizedSpeed = 0.0f;

    for(int32 I = 0; I < SampleDataList.Num(); ++I)
    {
        const int32 SampleDataIndex = SampleDataList[I].SampleDataIndex;        
        if(SampleData.IsValidIndex(SampleDataIndex))
        {
            const FBlendSample& Sample = SampleData[SampleDataIndex];
            if(Sample.Animation)
            {
                const float SampleAnimLength = Sample.GetSamplePlayLength();
                const float SampleWeight = SampleDataList[I].TotalWeight;
                
                const float SampleNormalizedSpeed = (SampleAnimLength > 0.0f) ? (1.0f / SampleAnimLength) : 0.0f;
                BlendedNormalizedSpeed += SampleNormalizedSpeed * SampleWeight;
            }
        }
    }

    if(BlendedNormalizedSpeed == 0.0f)
    {
        return 0.0f;
    }
    else
    {
        const float BlendedAnimLength = 1.0f / BlendedNormalizedSpeed;
        return BlendedAnimLength;
    }
}

void UBlendSpace::GetAnimationPose(TArray<FBlendSampleData>& BlendSampleDataCache, FCompactPose& OutPose, FBlendedCurve& OutCurve) const
{
    const int32 NumPoses = BlendSampleDataCache.Num();

    if(NumPoses == 0)
    {
        OutPose.ResetToRefPose();
        return;
    }

    TArray<FCompactPose> ChildrenPoses;
    ChildrenPoses.SetNum(NumPoses);

    TArray<FBlendedCurve> ChildrenCurves;
    ChildrenCurves.SetNum(NumPoses);

    TArray<float> ChildrenWeights;
    ChildrenWeights.SetNum(NumPoses);

    for(int32 ChildrenIdx = 0; ChildrenIdx < ChildrenPoses.Num(); ++ChildrenIdx)
    {
        ChildrenPoses[ChildrenIdx].SetRefSkeleton(OutPose.GetRefSkeleton());
    }
    
    for(int32 I = 0; I < NumPoses; ++I)
    {
        FCompactPose& Pose = ChildrenPoses[I];
        const int32 SampleIndex = BlendSampleDataCache[I].SampleDataIndex;

        if(SampleData.IsValidIndex(SampleIndex))
        {
            const FBlendSample& Sample = SampleData[BlendSampleDataCache[I].SampleDataIndex];
            ChildrenWeights[I] = BlendSampleDataCache[I].GetClampedWeight();

            if(Sample.Animation)
            {
                const float Time = FMath::Clamp<float>(BlendSampleDataCache[I].Time, 0.f, Sample.Animation->GetPlayLength());

                FAnimExtractContext ChildExtractionContext;
                ChildExtractionContext.CurrentTime = Time;
                Sample.Animation->GetAnimationPose(Pose, ChildExtractionContext);
            }
            else
            {
                Pose.ResetToRefPose();
            }
        }
    }

    if(NumPoses == 1)
    {
        OutPose = ChildrenPoses[0];
        OutCurve = ChildrenCurves[0];
        return;
    }
    
    const int32 NumBones = OutPose.GetNumBones();

    FAnimationRuntime::BlendPosesTogether(ChildrenPoses, ChildrenCurves, ChildrenWeights, OutPose, OutCurve);
}
