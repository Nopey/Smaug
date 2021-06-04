#pragma once

#include <bgfx/bgfx.h>
#include "shadermanager.h"
#include <glm/vec3.hpp>

class CWorldRenderer
{
public:
	CWorldRenderer();
	void Draw2D(bgfx::ViewId viewId, Shader shader);
	void Draw3D(bgfx::ViewId viewId, Shader shader);
	
};

class CNode;
glm::vec3 nodeColor(CNode* node);

// now defined in smaugapp.cpp
CWorldRenderer& GetWorldRenderer();
