#include "EnginePCH.h"
#include "AnimStateMachineTest.h"
#include "Engine/Classes/Animation/AnimNode_StateMachine.h"

#if defined(ENGINE_DEBUG)
namespace
{
	struct FNodeCalls
	{
		int32 Initialize = 0;
		int32 Update = 0;
		int32 Evaluate = 0;
		int32 Destroy = 0;
		float Time = 0.0f;
		FAnimationUpdateSharedContext* SharedContext = nullptr;
		int32 NodeId = -1;
	};

	struct FTestPoseNode : public FAnimNode_Base
	{
		FTestPoseNode(FNodeCalls& InCalls, float InOffset) : Calls(InCalls), Offset(InOffset) {}
		~FTestPoseNode() override { ++Calls.Destroy; }

		void Initialize(const FAnimationInitializeContext& Context) override
		{
			++Calls.Initialize;
			Calls.Time = 0.0f;
			Calls.SharedContext = Context.SharedContext;
			Calls.NodeId = Context.GetCurrentNodeId();
		}

		void Update(const FAnimationUpdateContext& Context) override
		{
			++Calls.Update;
			Calls.Time += Context.GetDeltaTime();
		}

		void Evaluate(FPoseContext& Output) override
		{
			++Calls.Evaluate;
			Output.Pose.ResetToRefPose();
			Output.Pose[0].Location.X = Offset + Calls.Time;
		}

		FNodeCalls& Calls;
		float Offset;
	};

	bool CheckNodeTest(bool bPassed, const char* Message)
	{
		if (!bPassed)
		{
			HTR_LOG(Error, "[AnimNodeSM] {}", Message);
		}
		return bPassed;
	}
}

bool AnimTest::VerifyAnimNodeStateMachine()
{
	FReferenceSkeleton Skeleton;
	Skeleton.RawRefBoneInfo.Add({ FName("Root"), -1 });
	Skeleton.RawRefBonePose.Add(FTransform());
	FPoseContext Output;
	Output.Pose.SetRefSkeleton(&Skeleton);
	FAnimationUpdateSharedContext SharedContext;
	FAnimationUpdateContext Context(nullptr, 0.25f, &SharedContext);
	Context.SetNodeId(37);
	FAnimationInitializeContext InitializeContext(Context);
	FNodeCalls LocomotionCalls, AttackCalls, HitCalls;
	bool bAttack = false;
	bool bHit = false;

	{
		FAnimNode_StateMachine Node;
		if (!CheckNodeTest(Node.AddState(FName("Invalid"), nullptr) == -1, "Null node registration")) return false;
		const int32 Locomotion = Node.AddState(FName("Locomotion"), MakeUnique<FTestPoseNode>(LocomotionCalls, 10.0f));
		const int32 Attack = Node.AddState(FName("Attack"), MakeUnique<FTestPoseNode>(AttackCalls, 20.0f));
		const int32 Hit = Node.AddState(FName("Hit"), MakeUnique<FTestPoseNode>(HitCalls, 30.0f));
		if (!CheckNodeTest(Node.AddTransition(Locomotion, Attack, [&bAttack]() { return bAttack; })
			&& Node.AddAnyStateTransition(Hit, [&bHit]() { return bHit; })
			&& Node.AddTransition(Hit, Locomotion, [&Node]() { return Node.GetStateElapsedTime() >= 0.5f; })
			&& Node.Start(Locomotion), "Node graph setup")) return false;

		Output.Pose[0].Location.X = 99.0f;
		Node.Update(Context);
		Node.Evaluate(Output);
		if (!CheckNodeTest(LocomotionCalls.Update == 0 && Output.Pose[0].Location.X == 0.0f,
			"Uninitialized node must not execute children")) return false;

		Node.Initialize(InitializeContext);
		Node.Update(Context);
		Node.Evaluate(Output);
		if (!CheckNodeTest(LocomotionCalls.Initialize == 1 && LocomotionCalls.Update == 1
			&& LocomotionCalls.Evaluate == 1 && AttackCalls.Update == 0 && HitCalls.Update == 0
			&& Output.Pose[0].Location.X == 10.25f, "Initial state routing and pose")) return false;

		bAttack = true;
		Node.Update(Context);
		Node.Evaluate(Output);
		bAttack = false;
		if (!CheckNodeTest(Node.GetCurrentState() == Attack && AttackCalls.Initialize == 1
			&& AttackCalls.Update == 1 && LocomotionCalls.Update == 1 && Output.Pose[0].Location.X == 20.25f
			&& AttackCalls.SharedContext == &SharedContext && AttackCalls.NodeId == 37,
			"Transition initialization, context and pose")) return false;

		bHit = true;
		Node.Update(Context);
		Node.Evaluate(Output);
		if (!CheckNodeTest(Node.GetCurrentState() == Hit && HitCalls.Initialize == 1
			&& HitCalls.Update == 1 && AttackCalls.Update == 1 && Output.Pose[0].Location.X == 30.25f,
			"Any State routing and pose")) return false;

		Node.Update(Context);
		if (!CheckNodeTest(HitCalls.Initialize == 1 && HitCalls.Time == 0.5f,
			"Disabled restart must preserve child playback")) return false;

		Node.SetStateRestartOnReentry(Hit, true);
		Node.Update(Context);
		bHit = false;
		if (!CheckNodeTest(HitCalls.Initialize == 2 && HitCalls.Time == 0.25f,
			"Same-state restart must initialize child")) return false;

		Node.Update(Context);
		Node.Update(Context);
		Node.Evaluate(Output);
		if (!CheckNodeTest(Node.GetCurrentState() == Locomotion && LocomotionCalls.Initialize == 2
			&& Output.Pose[0].Location.X == 10.25f, "Automatic return routing")) return false;

		Node.Initialize(InitializeContext);
		if (!CheckNodeTest(Node.GetCurrentState() == Locomotion && Node.GetStateElapsedTime() == 0.0f
			&& LocomotionCalls.Initialize == 3 && LocomotionCalls.Time == 0.0f,
			"Graph reinitialization must reset state and child")) return false;
	}

	if (!CheckNodeTest(LocomotionCalls.Destroy == 1 && AttackCalls.Destroy == 1 && HitCalls.Destroy == 1,
		"Owned child nodes must be destroyed")) return false;
	HTR_LOG(Info, "[AnimNodeSM] Node routing, pose and restart verification passed");
	return true;
}
#endif
