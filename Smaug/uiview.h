#pragma once
#include "baseview.h"
#include "3dview.h"
#include "editview.h"
#include "selectedview.h"
#include "toolbox.h"
#include "settingsmenu.h"

class CUIView : public CBaseView
{
public:
	virtual void Init(bgfx::ViewId viewId, int width, int height, uint32_t clearColor) override;
	virtual void Draw(float dt) override;
	virtual void Update(float dt, float mx, float my) override;
//private:
	bool m_drawEditView;
	CEditView m_editViews[3];
	bool m_drawPreviewView;
	C3DView m_previewView;
	bool m_drawSelectedView;
	CToolBox m_toolBox;
	CSettingsMenu m_settingsMenu;
};