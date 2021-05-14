#pragma once

#include "basetool.h"
#include "editoractions.h"

#include <GLFW/glfw3.h>

class CDragTool : public CBaseDragTool
{
public:
	CDragTool()
	{
		SetIconPath("assets/drag.png");
	}

	virtual const char* GetName() override { return "Drag"; }
	virtual const char* GetCursorPath() override { return "assets/extrude_gizmo.obj"; }

	// When this key is pressed, this tool becomes active
	virtual input_t GetToggleInput() override;

	// While this key is held, this tool is active
	virtual input_t GetHoldInput() override;


	virtual int GetSelectionType() override { return ACT_SELECT_SIDE /*| ACT_SELECT_VERT*/; }

	virtual void StartDrag() override
	{
		if (m_selectionInfo.selected & ACT_SELECT_VERT)
		{
			m_action = new CVertDragAction;
		}
		else
		{
			m_action = new CSideDragAction;
		}

		m_action->SetMoveStart(m_mouseStartDragPos);
		m_action->Select(m_selectionInfo);
	}

	virtual void EndDrag() override
	{
		if (glm::length(m_mouseDragDelta) == 0)
		{
			// No delta... Delete the action and move on
			delete m_action;
			m_action = nullptr;
		}
		else
		{
			m_action->SetMoveDelta(m_mouseDragDelta);
			GetActionManager().CommitAction(m_action);

			// We don't need to delete the action
			// The manager is taking care of it for us now
			m_action = nullptr;
		}
	};

	virtual void Preview() override
	{
		if (m_action)
		{
			m_action->SetMoveDelta(m_mouseDragDelta);
			m_action->Preview();
		}
	}

	CBaseDragAction* m_action;
};


class CExtrudeTool : public CBaseDragTool
{
public:

	CExtrudeTool()
	{
		SetIconPath("assets/extrude.png");
	}

	virtual const char* GetName() override { return "Extrude"; }
	virtual const char* GetCursorPath() override { return "assets/extend_gizmo.obj"; }

	// When this key is pressed, this tool becomes active
	virtual input_t GetToggleInput() override;

	// While this key is held, this tool is active
	virtual input_t GetHoldInput() override;


	virtual int GetSelectionType() override { return ACT_SELECT_SIDE; }

	virtual void StartDrag() override
	{
		m_wallExtrudeAction = new CWallExtrudeAction;
		m_wallExtrudeAction->Select(m_selectionInfo);
		m_wallExtrudeAction->SetMoveStart(m_mouseStartDragPos);
	}

	virtual void EndDrag() override
	{
		if (glm::length(m_mouseDragDelta) == 0)
		{
			// No delta... Delete the action and move on
			delete m_wallExtrudeAction;
			m_wallExtrudeAction = nullptr;
		}
		else if (0)//glm::dot(faceNormal(m_selectionInfo.side), m_mouseDragDelta) >= 0)
		{
			// Backwards extrude... Delete and move on
			delete m_wallExtrudeAction;
			m_wallExtrudeAction = nullptr;
		}
		else
		{
			m_wallExtrudeAction->SetMoveDelta(m_mouseDragDelta);
			GetActionManager().CommitAction(m_wallExtrudeAction);
			// We don't need to delete the action
			// The manager is taking care of it for us now
			m_wallExtrudeAction = nullptr;
		}

	};

	virtual void Preview() override
	{
		if (m_wallExtrudeAction)
		{
			m_wallExtrudeAction->SetMoveDelta(m_mouseDragDelta);
			m_wallExtrudeAction->Preview();
		}
	}

	CWallExtrudeAction* m_wallExtrudeAction;
};
