#pragma once

#include "actionmanager.h"
#include "texturemanager.h"
#include "input.h"
#include <glm/vec3.hpp>

class CBaseTool
{
public:
	CBaseTool();

	// Things are abstract here to force the programmer to set them later.

	virtual const char* GetName() = 0;
	virtual const char* GetCursorPath() = 0;
	
	bgfx::TextureHandle GetIconTexture() { return m_iconHandle; }

	// When this key is pressed, this tool becomes active
	virtual input_t GetToggleInput() = 0;

	// While this key is held, this tool is active
	virtual input_t GetHoldInput() = 0;

	virtual void Enable();
	// Mouse pos is snapped to grid, raw is not. Use raw for selections and snapped for actions
	virtual void Update(float dt, glm::vec3 mousePosSnapped, glm::vec3 mousePosRaw) {};
	virtual void Disable() {};


protected:

	inline void SetIconPath(char const* path)
	{
		m_iconHandle = TextureManager().LoadTexture(path);
	}
	bgfx::TextureHandle m_iconHandle;

private:
};


// This base tool implements basic element selection
class CBaseSelectionTool : public CBaseTool
{
public:

	// Override this with your ACT_SELECT_ to request types for selection
	virtual int GetSelectionType() = 0;
};

// This base tool implements basic mouse dragging
class CBaseDragTool : public CBaseSelectionTool
{
public:

	virtual void Enable();
	virtual void Update(float dt, glm::vec3 mousePosSnapped, glm::vec3 mousePosRaw);

	virtual void Preview() = 0;
	virtual void StartDrag() = 0;
	virtual void EndDrag() = 0;

	bool m_inDrag;
	glm::vec3 m_mouseStartDragPos;
	glm::vec3 m_mouseDragDelta;
	selectionInfo_t m_selectionInfo;
};