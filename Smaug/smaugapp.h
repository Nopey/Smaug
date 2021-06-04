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

	// These members all store a pointer to `this` in a static global,
	// and define their own getter.
	CActionManager m_actionManager;
	CModelManager m_modelManager;
	CShaderManager m_shaderManager;
	CTextureManager m_textureManager;
	CWorldEditor m_worldEditor;
	CSelectedView m_selectedView;
	
	// These members all have static getters defined in smaugapp.cpp
	CBasicDraw m_basicDraw;
	CCursor m_cursor;
	CDebugDraw m_debugDraw;
	CGrid m_grid;
	CInputManager m_input;
	CTextureBrowser m_textureBrowser;
	CWorldRenderer m_worldRenderer;
};

CSmaugApp& GetApp();
