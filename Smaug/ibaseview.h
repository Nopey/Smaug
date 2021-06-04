#pragma once
#include <bgfx/bgfx.h>
#include <imgui.h>

class IBaseView
{
public:
	virtual void Draw(float dt) = 0;
	virtual void Update(float dt, float mx, float my) = 0; // Having mouse input input here might be odd, but I'm not sure yet
	virtual ImTextureID GetImTextureID() = 0;
};
