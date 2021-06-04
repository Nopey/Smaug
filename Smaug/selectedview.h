#pragma once
#include "baseview.h"
#include "worldeditor.h"

class CSelectedView : public CBaseView
{
public:
	CSelectedView(bgfx::ViewId viewId, int width, int height, uint32_t clearColor);
	~CSelectedView();

	virtual void Draw(float dt);
	void SetSelection(CNodeRef node);
	bool Show();
private:
	CNodeRef m_selectedNode;
	float m_aabbLength;
};

CSelectedView& SelectedView();
