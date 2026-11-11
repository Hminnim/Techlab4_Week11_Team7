#include "EnginePCH.h"
#include "Editor/AnimationStateMachine/AnimationStateMachinePanel.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Classes/Animation/AnimInstance.h"
#include "Engine/Classes/Animation/AnimNode_BlendSpacePlayer.h"
#include "Engine/Classes/Animation/AnimNode_SequencePlayer.h"
#include "Engine/Classes/Animation/AnimNode_StateMachine.h"
#include "Engine/Classes/Animation/AnimNode_TwoWayBlend.h"
#include "GameFramework/Actor.h"

#include <algorithm>
#include <imgui_node_editor.h>

namespace NodeEditor = ax::NodeEditor;

namespace
{
	constexpr uintptr_t NodeIdStride = 4; // Node, two inputs, output
	constexpr ImVec4 TransitionColor(0.58f, 0.72f, 0.9f, 1.0f);
	constexpr ImVec4 PoseColor(0.9f, 0.9f, 0.9f, 1.0f);
	constexpr ImVec4 NodeTitleColor(0.73f, 0.82f, 0.95f, 1.0f);

	const UAnimInstance* FindAnimInstance(const AActor* Actor)
	{
		if (!Actor) return nullptr;

		for (const UActorComponent* Component : Actor->GetComponents())
		{
			if (const auto* Mesh = Cast<USkeletalMeshComponent>(Component))
			{
				const UAnimInstance* Instance = Mesh->GetAnimInstance();
				if (Instance && Instance->GetRootNode()) return Instance;
			}
		}
		return nullptr;
	}

	NodeEditor::NodeId GetNodeId(int32 Index)
	{
		return static_cast<uintptr_t>(Index) * NodeIdStride + 1;
	}

	NodeEditor::PinId GetPinId(int32 Index, NodeEditor::PinKind Kind, int32 InputIndex = 0)
	{
		return GetNodeId(Index).Get() + (Kind == NodeEditor::PinKind::Input ? 1 + InputIndex : 3);
	}

	void DrawPin(int32 Index, NodeEditor::PinKind Kind, int32 InputIndex = 0, bool bArrow = false)
	{
		if (bArrow)
		{
			NodeEditor::PushStyleVar(NodeEditor::StyleVar_PinArrowSize, 9.0f);
			NodeEditor::PushStyleVar(NodeEditor::StyleVar_PinArrowWidth, 7.0f);
			NodeEditor::PushStyleVar(NodeEditor::StyleVar_SnapLinkToPinDir, 1.0f);
		}
		NodeEditor::BeginPin(GetPinId(Index, Kind, InputIndex), Kind);
		// Extend the pivot by one pin width to attach the wire to the node border.
		NodeEditor::PinPivotAlignment(ImVec2(Kind == NodeEditor::PinKind::Input ? -1.0f : 2.0f, 0.5f));
		ImGui::Dummy(ImVec2(8.0f, ImGui::GetTextLineHeight()));
		NodeEditor::EndPin();
		if (bArrow) NodeEditor::PopStyleVar(3);
	}

	FString GetClipName(const UAnimSequence* Sequence)
	{
		if (!Sequence) return "None";
		const FString& Path = Sequence->GetPath();
		if (Path.empty()) return Sequence->GetName();
		const size_t Separator = Path.find_last_of("/\\");
		return Separator == FString::npos ? Path : Path.substr(Separator + 1);
	}

	void DrawPoseNodeContent(const FAnimNode_Base& Node)
	{
		if (const auto* Machine = dynamic_cast<const FAnimNode_StateMachine*>(&Node))
		{
			ImGui::TextColored(NodeTitleColor, "State Machine");
			ImGui::Text("Active: %s", Machine->GetCurrentStateName().ToString().c_str());
			ImGui::TextDisabled("Double-click to open");
		}
		else if (const auto* Player = dynamic_cast<const FAnimNode_SequencePlayer*>(&Node))
		{
			const UAnimSequence* Sequence = Player->GetSequence();
			ImGui::TextColored(NodeTitleColor, "Sequence Player");
			ImGui::TextUnformatted(GetClipName(Sequence).c_str());
			if (Sequence)
			{
				ImGui::TextDisabled("%.2f / %.2f s%s", Player->GetCurrentPlayTime(), Sequence->GetPlayLength(),
					Player->IsLoopAnimation() ? "  Loop" : "");
			}
		}
		else if (const auto* BlendSpacePlayer = dynamic_cast<const FAnimNode_BlendSpacePlayer*>(&Node))
		{
			ImGui::TextColored(NodeTitleColor, "Blend Space Player");
			ImGui::Text("X: %.2f", BlendSpacePlayer->X);
			if (BlendSpacePlayer->BlendSpace)
			{
				for (const FBlendSample& Sample : BlendSpacePlayer->BlendSpace->SampleData)
				{
					ImGui::Text("%.2f  %s", Sample.SamepleVector.X, GetClipName(Sample.Animation).c_str());
				}
			}
		}
		else if (const auto* Blend = dynamic_cast<const FAnimNode_TwoWayBlend*>(&Node))
		{
			ImGui::TextColored(NodeTitleColor, "Two Way Blend");
			ImGui::Text("Alpha: %.2f", Blend->Alpha);
		}
		else
		{
			ImGui::TextUnformatted("Pose Node");
		}
	}

	TArray<const FAnimNode_Base*> DrawPoseGraph(const FAnimNode_Base& Root, bool bNewGraph)
	{
		TArray<const FAnimNode_Base*> Nodes{ &Root };
		TArray<int32> Depths{ 0 };
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (const auto* Blend = dynamic_cast<const FAnimNode_TwoWayBlend*>(Nodes[Index]))
			{
				for (const FAnimNode_Base* Input : { Blend->A.LinkedNode, Blend->B.LinkedNode })
				{
					if (Input && Nodes.Find(Input) == INDEX_NONE)
					{
						const int32 Depth = Depths[Index] + 1;
						Nodes.Add(Input);
						Depths.Add(Depth);
					}
				}
			}
		}

		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			const auto Id = GetNodeId(Index);
			if (bNewGraph)
			{
				NodeEditor::SetNodePosition(Id, ImVec2(40.0f - 280.0f * Depths[Index], 60.0f + 150.0f * Index));
			}
			NodeEditor::BeginNode(Id);
			if (dynamic_cast<const FAnimNode_TwoWayBlend*>(Nodes[Index]))
			{
				ImGui::BeginGroup();
				DrawPin(Index, NodeEditor::PinKind::Input, 0);
				DrawPin(Index, NodeEditor::PinKind::Input, 1);
				ImGui::EndGroup();
				ImGui::SameLine();
			}
			ImGui::BeginGroup();
			DrawPoseNodeContent(*Nodes[Index]);
			ImGui::EndGroup();
			ImGui::SameLine();
			DrawPin(Index, NodeEditor::PinKind::Output);
			NodeEditor::EndNode();
		}

		const int32 OutputIndex = Nodes.Num();
		const auto OutputId = GetNodeId(OutputIndex);
		if (bNewGraph) NodeEditor::SetNodePosition(OutputId, ImVec2(330.0f, 60.0f));
		NodeEditor::BeginNode(OutputId);
		DrawPin(OutputIndex, NodeEditor::PinKind::Input);
		ImGui::SameLine();
		ImGui::TextColored(NodeTitleColor, "Output Pose");
		NodeEditor::EndNode();

		uintptr_t LinkId = (static_cast<uintptr_t>(OutputIndex) + 1) * NodeIdStride + 1;
		NodeEditor::Link(LinkId++, GetPinId(0, NodeEditor::PinKind::Output),
			GetPinId(OutputIndex, NodeEditor::PinKind::Input), PoseColor, 3.0f);
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (const auto* Blend = dynamic_cast<const FAnimNode_TwoWayBlend*>(Nodes[Index]))
			{
				const FAnimNode_Base* Inputs[] = { Blend->A.LinkedNode, Blend->B.LinkedNode };
				for (int32 InputIndex = 0; InputIndex < 2; ++InputIndex)
				{
					const int32 SourceIndex = Nodes.Find(Inputs[InputIndex]);
					if (SourceIndex == INDEX_NONE) continue;
					NodeEditor::Link(LinkId++, GetPinId(SourceIndex, NodeEditor::PinKind::Output),
						GetPinId(Index, NodeEditor::PinKind::Input, InputIndex), PoseColor, 3.0f);
				}
			}
		}
		return Nodes;
	}

	void DrawStateMachine(const FAnimStateMachine& Machine, bool bNewGraph)
	{
		const auto& States = Machine.GetStates();
		const auto& Transitions = Machine.GetTransitions();
		const int32 StateCount = States.Num();
		const bool bHasAnyState = std::any_of(Transitions.begin(), Transitions.end(),
			[](const FAnimTransitionDesc& Transition) { return Transition.FromState == -1; });
		const int32 EntryIndex = StateCount + bHasAnyState;
		const bool bHasEntry = States.IsValidIndex(Machine.GetInitialState());
		const int32 NodeCount = EntryIndex + bHasEntry;

		for (int32 Index = 0; Index < NodeCount; ++Index)
		{
			const auto Id = GetNodeId(Index);
			const bool bState = Index < StateCount;
			const bool bActive = bState && Index == Machine.GetCurrentState();
			if (bNewGraph)
			{
				const int32 Slot = Index == EntryIndex ? 0 : Index + bHasEntry;
				NodeEditor::SetNodePosition(Id, ImVec2(32.0f + 180.0f * (Slot % 3), 48.0f + 90.0f * (Slot / 3)));
			}
			if (bActive)
			{
				NodeEditor::PushStyleColor(NodeEditor::StyleColor_NodeBg, ImVec4(0.12f, 0.29f, 0.23f, 1.0f));
				NodeEditor::PushStyleColor(NodeEditor::StyleColor_NodeBorder, ImVec4(0.35f, 0.85f, 0.56f, 1.0f));
			}
			NodeEditor::BeginNode(Id);
			if (bState)
			{
				DrawPin(Index, NodeEditor::PinKind::Input, 0, true);
				ImGui::SameLine();
			}
			const FString Label = bState ? States[Index].Name.ToString() : (Index == EntryIndex ? "Entry" : "Any State");
			ImGui::TextUnformatted(Label.c_str());
			ImGui::SameLine();
			DrawPin(Index, NodeEditor::PinKind::Output);
			NodeEditor::EndNode();
			if (bActive) NodeEditor::PopStyleColor(2);
		}

		uintptr_t LinkId = static_cast<uintptr_t>(NodeCount) * NodeIdStride + 1;
		if (bHasEntry)
		{
			NodeEditor::Link(LinkId++, GetPinId(EntryIndex, NodeEditor::PinKind::Output),
				GetPinId(Machine.GetInitialState(), NodeEditor::PinKind::Input), TransitionColor, 2.0f);
		}
		for (const FAnimTransitionDesc& Transition : Transitions)
		{
			const int32 SourceIndex = Transition.FromState == -1 ? StateCount : Transition.FromState;
			NodeEditor::Link(LinkId++, GetPinId(SourceIndex, NodeEditor::PinKind::Output),
				GetPinId(Transition.ToState, NodeEditor::PinKind::Input), TransitionColor, 2.0f);
		}
	}
}

FAnimationStateMachinePanel::~FAnimationStateMachinePanel()
{
	Shutdown();
}

bool FAnimationStateMachinePanel::Init()
{
	SetOpen(false);
	return true;
}

void FAnimationStateMachinePanel::Shutdown()
{
	for (auto* Canvases : { &PoseCanvases, &StateMachineCanvases })
	{
		for (const auto& Canvas : *Canvases) NodeEditor::DestroyEditor(Canvas.second);
		Canvases->Reset();
	}
	Pages.Reset();
}

void FAnimationStateMachinePanel::OnRender()
{
	bool bOpen = IsOpen();
	ImGui::SetNextWindowSize(ImVec2(720.0f, 480.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(GetPanelName(), &bOpen))
	{
		const AActor* Actor = SelectedActorQuery ? SelectedActorQuery() : nullptr;
		if (const UAnimInstance* Instance = FindAnimInstance(Actor))
		{
			DrawAnimGraph(*Instance);
		}
		else
		{
			ImGui::TextWrapped("Play the scene and select a character with an animation graph in the Outliner.");
		}
	}
	ImGui::End();
	SetOpen(bOpen);
}

void FAnimationStateMachinePanel::DrawAnimGraph(const UAnimInstance& AnimInstance)
{
	const FAnimNode_Base* Root = AnimInstance.GetRootNode();
	if (Pages.IsEmpty() || AnimInstanceId != AnimInstance.GetUUID() || Pages[0].Root != Root)
	{
		Shutdown();
		AnimInstanceId = AnimInstance.GetUUID();
		Pages.Add({ Root, "AnimGraph" });
	}

	for (int32 Index = 0; Index < Pages.Num(); ++Index)
	{
		if (Index == Pages.Num() - 1)
		{
			ImGui::TextUnformatted(Pages[Index].Name.c_str());
			break;
		}
		ImGui::PushID(Index);
		const bool bClicked = ImGui::SmallButton(Pages[Index].Name.c_str());
		ImGui::PopID();
		if (bClicked)
		{
			Pages.SetNum(Index + 1);
			break;
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(">");
		ImGui::SameLine();
	}
	const FGraphPage& Page = Pages.Last();

	const auto* MachineNode = Page.bStateMachine ? static_cast<const FAnimNode_StateMachine*>(Page.Root) : nullptr;
	if (MachineNode)
	{
		ImGui::Text("Current state: %s (%.2f s)", MachineNode->GetCurrentStateName().ToString().c_str(),
			MachineNode->GetStateElapsedTime());
		ImGui::TextDisabled("Double-click a state to view its pose graph.");
	}
	ImGui::Separator();

	auto& Canvases = MachineNode ? StateMachineCanvases : PoseCanvases;
	auto*& Context = Canvases.FindOrAdd(Page.Root);
	const bool bNewGraph = !Context;
	if (bNewGraph)
	{
		NodeEditor::Config Config;
		Config.SettingsFile = nullptr;
		Context = NodeEditor::CreateEditor(&Config);
	}
	NodeEditor::SetCurrentEditor(Context);
	if (bNewGraph) NodeEditor::EnableShortcuts(false);
	NodeEditor::Begin("AnimGraphCanvas");
	TArray<const FAnimNode_Base*> PoseNodes;
	if (MachineNode)
	{
		DrawStateMachine(MachineNode->GetStateMachine(), bNewGraph);
	}
	else
	{
		PoseNodes = DrawPoseGraph(*Page.Root, bNewGraph);
	}
	NodeEditor::End();
	if (bNewGraph) NodeEditor::NavigateToContent();
	const auto ClickedNode = NodeEditor::GetDoubleClickedNode();
	NodeEditor::SetCurrentEditor(nullptr);

	if (!ClickedNode) return;
	const int32 Index = static_cast<int32>((ClickedNode.Get() - 1) / NodeIdStride);
	if (MachineNode)
	{
		if (const FAnimNode_Base* StateRoot = MachineNode->GetStatePoseNode(Index))
		{
			Pages.Add({ StateRoot, MachineNode->GetStateMachine().GetStates()[Index].Name.ToString() });
		}
	}
	else if (PoseNodes.IsValidIndex(Index))
	{
		if (const auto* StateMachine = dynamic_cast<const FAnimNode_StateMachine*>(PoseNodes[Index]))
		{
			Pages.Add({ StateMachine, "State Machine", true });
		}
	}
}
