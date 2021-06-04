#pragma once
#include <bigg.hpp>
#include "uiview.h"

#include "actionmanager.h"
#include "basicdraw.h"
#include "cursor.h"
#include "debugdraw.h"
#include "grid.h"
#include "input.h"
#include "modelmanager.h"
#include "selectedview.h"
#include "shadermanager.h"
#include "texturebrowser.h"
#include "texturemanager.h"
#include "worldeditor.h"
#include "worldrenderer.h"

class CSmaugApp : public bigg::Application
{
public:

	CSmaugApp();

	void initialize(int _argc, char** _argv);
	int  shutdown();
	void update(float dt);
	void onReset();

	void SetMouseLock(bool state);
	void ToggleMouseLock();
	
	GLFWwindow* GetWindow() { return mWindow; };


	CUIView m_uiView;
	bool m_mouseLocked;

	// TODO: Order app members so they construct in the proper order, and make some sense.
	// Currently they're alphabetical
	CActionManager m_actionManager;
	CBasicDraw m_basicDraw;
	CCursor m_cursor;
	CDebugDraw m_debugDraw;
	CGrid m_grid;
	CInputManager m_input;
	CModelManager m_modelManager;
	CSelectedView m_selectedView;
	CShaderManager m_shaderManager;
	CTextureBrowser m_textureBrowser;
	CTextureManager m_textureManager;
	CWorldEditor m_worldEditor;
	CWorldRenderer m_worldRenderer;
};

CSmaugApp& GetApp();
