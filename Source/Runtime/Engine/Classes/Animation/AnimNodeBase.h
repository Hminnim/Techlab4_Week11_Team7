#pragma once

#include "Core/Types.h"
#include "AnimNodeData.h"
#include "AnimCurveTypes.h"
#include "BonePose.h"

struct FAnimNode_Base;
struct FAnimInstanceProxy;
class UAnimInstance;

struct FAnimationUpdateSharedContext
{
	FAnimationUpdateSharedContext() = default;

	FAnimationUpdateSharedContext(FAnimationUpdateSharedContext&) = default;
	FAnimationUpdateSharedContext& operator=(const FAnimationUpdateSharedContext&) = delete;

	// MessageStack


};

struct FAnimationBaseContext
{
	FAnimInstanceProxy* AnimInstanceProxy = nullptr;

	FAnimationUpdateSharedContext* SharedContext = nullptr;

	UAnimInstance* AnimInstance = nullptr;

	FAnimationBaseContext();

protected:	

	FAnimationBaseContext(UAnimInstance* InAnimInstance);
	FAnimationBaseContext(FAnimInstanceProxy* InAnimInstanceProxy, FAnimationUpdateSharedContext* InSharedContext = nullptr);

public:
	FAnimationBaseContext(FAnimationBaseContext&&) = default;
	FAnimationBaseContext(const FAnimationBaseContext&) = default;
	FAnimationBaseContext& operator=(FAnimationBaseContext&&) = default;
	FAnimationBaseContext& operator=(const FAnimationBaseContext&) = default;

public:
	UObject* GetAnimInstanceObject() const;         
	UAnimInstance* GetAnimInstance() const { return AnimInstance; }

	void SetNodeId(int32 InNodeId)
	{
		PreviousNodeId = CurrentNodeId;
		CurrentNodeId = InNodeId;
	}

	void SetNodeIds(const FAnimationBaseContext& InContext)
	{
		CurrentNodeId = InContext.CurrentNodeId;
		PreviousNodeId = InContext.PreviousNodeId;
	}

	int32 GetCurrentNodeId() const { return CurrentNodeId; }
	int32 GetPreviouseNodeId() const { return PreviousNodeId; }
	bool IsActive() const { return bIsActive; }

protected:
	bool bIsActive = true;
	int32 CurrentNodeId = -1;
	int32 PreviousNodeId = -1;	
};

struct FAnimationInitializeContext : public FAnimationBaseContext
{
public:
	explicit FAnimationInitializeContext(const FAnimationBaseContext& InContext)
		: FAnimationBaseContext(InContext)
	{
	}

	FAnimationInitializeContext(FAnimInstanceProxy* InAnimInstanceProxy, FAnimationUpdateSharedContext* InSharedContext = nullptr)
		: FAnimationBaseContext(InAnimInstanceProxy, InSharedContext)
	{

	}

	FAnimationInitializeContext(UAnimInstance* InAnimInstance)
		: FAnimationBaseContext(InAnimInstance)
	{

	}
};

struct FAnimationUpdateContext : public FAnimationBaseContext
{
private:
	float CurrentWeight;
	float RootMotionWeightModifier;

	float DeltaTime;

public:
	FAnimationUpdateContext(FAnimInstanceProxy* InAnimInstanceProxy = nullptr)
		: FAnimationBaseContext(InAnimInstanceProxy)
		, CurrentWeight(1.0f)
		, RootMotionWeightModifier(1.0f)
		, DeltaTime(0.0f)
	{
	}

	FAnimationUpdateContext(FAnimInstanceProxy* InAnimInstanceProxy, float InDeltaTime, FAnimationUpdateSharedContext* InSharedContext = nullptr)
		: FAnimationBaseContext(InAnimInstanceProxy, InSharedContext)
		, CurrentWeight(1.0f)
		, RootMotionWeightModifier(1.0f)
		, DeltaTime(InDeltaTime)
	{
	}


	FAnimationUpdateContext(const FAnimationUpdateContext& Copy, FAnimInstanceProxy* InAnimInstanceProxy)
		: FAnimationBaseContext(InAnimInstanceProxy, Copy.SharedContext)		
		, CurrentWeight(Copy.CurrentWeight)
		, RootMotionWeightModifier(Copy.RootMotionWeightModifier)
		, DeltaTime(Copy.DeltaTime)
	{
		CurrentNodeId = Copy.CurrentNodeId;
		PreviousNodeId = Copy.PreviousNodeId;
		AnimInstance = Copy.AnimInstance;
	}

	FAnimationUpdateContext(UAnimInstance* InAnimInstance)
	{
		CurrentWeight = 1.0f;
		RootMotionWeightModifier = 1.0f;
		DeltaTime = 0.0f;
		AnimInstance = InAnimInstance;
	}

	FAnimationUpdateContext(UAnimInstance* InAnimInstance, float InDeltaTime)
	{
		CurrentWeight = 1.0f;
		RootMotionWeightModifier = 1.0f;
		DeltaTime = InDeltaTime;
		AnimInstance = InAnimInstance;
	}

public:
	FAnimationUpdateContext WithOtherProxy(FAnimInstanceProxy* InAnimInstanceProxy) const
	{
		return FAnimationUpdateContext(*this, InAnimInstanceProxy);
	}

	FAnimationUpdateContext WithOtherSharedContext(FAnimationUpdateSharedContext* InSharedContext) const
	{
		FAnimationUpdateContext Result(*this);
		Result.SharedContext = InSharedContext;		

		return Result;
	}

	FAnimationUpdateContext AsInactive() const
	{
		FAnimationUpdateContext Result(*this);
		Result.bIsActive = false;

		return Result;
	}

	FAnimationUpdateContext FractionalWeight(float WeightMultiplier) const
	{
		FAnimationUpdateContext Result(*this);
		Result.CurrentWeight = CurrentWeight * WeightMultiplier;

		return Result;
	}

	FAnimationUpdateContext FractionalWeightAndRootMotion(float WeightMultiplier, float RootMotionMultiplier) const
	{
		FAnimationUpdateContext Result(*this);
		Result.CurrentWeight = CurrentWeight * WeightMultiplier;
		Result.RootMotionWeightModifier = RootMotionWeightModifier * RootMotionMultiplier;

		return Result;
	}

	FAnimationUpdateContext FractionalWeightAndTime(float WeightMultiplier, float TimeMultiplier) const
	{
		FAnimationUpdateContext Result(*this);
		Result.DeltaTime = DeltaTime * TimeMultiplier;
		Result.CurrentWeight = CurrentWeight * WeightMultiplier;
		return Result;
	}

	FAnimationUpdateContext FractionalWeightTimeAndRootMotion(float WeightMultiplier, float TimeMultiplier, float RootMotionMultiplier) const
	{
		FAnimationUpdateContext Result(*this);
		Result.DeltaTime = DeltaTime * TimeMultiplier;
		Result.CurrentWeight = CurrentWeight * WeightMultiplier;
		Result.RootMotionWeightModifier = RootMotionWeightModifier * RootMotionMultiplier;

		return Result;
	}

	FAnimationUpdateContext WithNodeId(int32 InNodeId) const
	{
		FAnimationUpdateContext Result(*this);
		Result.SetNodeId(InNodeId);
		return Result;
	}
	
	FAnimationUpdateSharedContext* GetSharedContext() const
	{
		return SharedContext;
	}
	float GetFinalBlendWeight() const { return CurrentWeight; }	
	float GetRootMotionWeightModifier() const { return RootMotionWeightModifier; }	
	float GetDeltaTime() const { return DeltaTime; }
};

struct FPoseContext : public FAnimationBaseContext
{
public:
	FCompactPose Pose;
	FBlendedCurve Curve;
};

struct FPoseLink
{
	FAnimNode_Base* LinkedNode;
	int32 LinkID;
	
public:
	FPoseLink() : LinkedNode(nullptr), LinkID(INDEX_NONE) {}
	
	void Initialize(const FAnimationInitializeContext& Context);
	void Update(const FAnimationUpdateContext& Context);
	void Evaluate(FPoseContext& Output);
	void SetLinkNode(FAnimNode_Base* NewLinkNode);
	FAnimNode_Base* GetLinkNode() { return LinkedNode; }
};

struct FAnimNode_Base
{
	virtual ~FAnimNode_Base() = default;
	virtual void Initialize(const FAnimationInitializeContext& Context);
	virtual void Update(const FAnimationUpdateContext& Context);
	virtual void Evaluate(FPoseContext& Output);
};