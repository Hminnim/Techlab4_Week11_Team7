#pragma once

#include "Editor/EditorUI/EditorPanel.h"

// 애니메이션 시퀀스 에셋을 위한 독립 에디터 패널.
class FAnimationSequenceEditorPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return "Animation Sequence Editor"; }
};
