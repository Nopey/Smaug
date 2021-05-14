#pragma once
#include "actionmanager.h"


class CVertDragAction : public CBaseDragAction
{
public:
	virtual const char* GetName() override { return "Drag Vertex"; }

	virtual void Select(selectionInfo_t selectInfo) override
	{
		CBaseDragAction::Select(selectInfo);
		m_originalPos = *m_selectInfo.vertex->vert;
	}

	virtual void Preview() override
	{
		*m_selectInfo.vertex->vert = m_originalPos + m_moveDelta;
		m_node->PreviewUpdate();
	}

	virtual void Act() override
	{
		*m_selectInfo.vertex->vert = m_originalPos + m_moveDelta;
		m_node->Update();
		m_finalMoveDelta = m_moveDelta;
	}

	virtual void Undo() override
	{
		*m_selectInfo.vertex->vert -= m_finalMoveDelta;
		m_node->Update();
	}

	virtual void Redo() override
	{
		*m_selectInfo.vertex->vert += m_finalMoveDelta;
		m_node->Update();
	}

#if 0 // disabled until IAction regains Cancel method
	virtual void Cancel() override
	{
		*m_selectInfo.vertex->vert = m_originalPos;
		m_node->Update();
	}
#endif

private:
	glm::vec3 m_originalPos;
	glm::vec3 m_finalMoveDelta;

	virtual int GetSelectionType() override;
};


class CWallExtrudeAction : public CBaseDragAction
{
public:
	virtual const char* GetName() override { return "Extrude Wall"; }

	CNode* CreateExtrusion();

	virtual void Preview() override;
	virtual void Act() override;
	virtual void Undo() override;
	virtual void Redo() override;

	virtual int GetSelectionType() override
	{
		return ACT_SELECT_SIDE;
	}

	CNodeRef m_quad;
};

class CSideDragAction : public CBaseDragAction
{
public:
	~CSideDragAction() override
	{
		if (m_originalPos)
			delete[] m_originalPos;
	}

	virtual const char* GetName() override { return "Drag Side"; }
	
	virtual void Select(selectionInfo_t selectInfo) override
	{
		CBaseDragAction::Select(selectInfo);
		if (!m_originalPos)
		{

			m_originalPos = new glm::vec3[selectInfo.side->verts.size()];
		}
		else
			printf("Yo! Bad call!");

		for (int i = 0; auto v : selectInfo.side->verts)
			m_originalPos[i++] = *v->vert;
	}

	virtual void Preview() override
	{
		for (int i = 0; auto v : m_selectInfo.side->verts)
			*v->vert = m_originalPos[i++] + m_moveDelta;

		m_node->PreviewUpdate();
	}

	virtual void Act() override
	{
		for (int i = 0; auto v : m_selectInfo.side->verts)
			*v->vert = m_originalPos[i++] + m_moveDelta;
		m_node->Update();

		m_finalMoveDelta = m_moveDelta;
	}

	virtual void Undo() override
	{
		for (int i = 0; auto v : m_selectInfo.side->verts)
			*v->vert -= m_finalMoveDelta;
		m_node->Update();
	}

	virtual void Redo() override
	{
		for (int i = 0; auto v : m_selectInfo.side->verts)
			*v->vert += m_finalMoveDelta;
		m_node->Update();
	}

#if 0 // disabled until IAction regains Cancel method
	virtual void Cancel() override
	{
		for (int i = 0; auto v : m_selectInfo.side->verts)
			*v->vert = m_originalPos[i++];
		m_node->Update();
	}
#endif

	virtual int GetSelectionType() override
	{
		return ACT_SELECT_SIDE;
	}

	glm::vec3* m_originalPos = nullptr;
	glm::vec3 m_finalMoveDelta;

};
