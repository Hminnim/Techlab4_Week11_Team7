#include "EnginePCH.h"
#include "Editor/AnimationSequenceEditor/AnimationSequenceEditorPanel.h"

bool FAnimationSequenceEditorPanel::Init()
{
	SetOpen(false);
	return true;
}

void FAnimationSequenceEditorPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 700.0f), ImGuiCond_FirstUseEver);

	bool bPanelOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bPanelOpen))
	{
		ImGui::TextUnformatted("No animation sequence selected.");
	}
	ImGui::End();

	SetOpen(bPanelOpen);
}