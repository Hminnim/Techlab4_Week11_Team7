#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Editor/EditorUI/EditorPanel.h"

#include <functional>
#include <utility>

class AActor;
class UAnimInstance;
struct FAnimNode_Base;

namespace ax::NodeEditor
{
	struct EditorContext;
}

class FAnimationStateMachinePanel : public IEditorPanel
{
public:
	~FAnimationStateMachinePanel() override;
	bool Init() override;
	void Tick(float) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return "AnimGraph"; }
	void SetSelectedActorQuery(std::function<const AActor*()> Query) { SelectedActorQuery = std::move(Query); }
	void Shutdown();

private:
	struct FGraphPage
	{
		const FAnimNode_Base* Root;
		FString Name;
		bool bStateMachine = false;
	};

	void DrawAnimGraph(const UAnimInstance& AnimInstance);

	std::function<const AActor*()> SelectedActorQuery;
	TArray<FGraphPage> Pages;
	TMap<const FAnimNode_Base*, ax::NodeEditor::EditorContext*> PoseCanvases;
	TMap<const FAnimNode_Base*, ax::NodeEditor::EditorContext*> StateMachineCanvases;
	uint32 AnimInstanceId = 0;
};
