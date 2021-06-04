#include "editoractions.h"
#include "debugdraw.h"
#include <utils.h>

std::unique_ptr<CNode> CWallExtrudeAction::CreateExtrusion()
{
	auto quad = std::make_unique<CQuadNode>();
	quad->m_mesh.origin = m_node->Origin();

	meshPart_t* selectedPart = m_selectInfo.side;

	meshPart_t* newFront = quad->m_mesh.parts[0];
	meshPart_t* newBack = quad->m_mesh.parts[1];

	for (int i = 0; auto v : selectedPart->verts)
		*newBack->verts[i++]->vert = *v->vert + m_moveDelta;

	for (int i = 0; auto v : selectedPart->verts)
		*newFront->verts[newFront->verts.size() - 1 - (i++)]->vert = *v->vert;

	return quad;
}

void CWallExtrudeAction::Preview()
{
	meshPart_t* selectedPart = m_selectInfo.side;
	glm::vec3 color = glm::dot(selectedPart->normal, m_moveDelta) < 0 ? COLOR_BLUE : COLOR_RED;
	for(auto v : selectedPart->verts)
		DebugDraw().LineDelta(*v->vert + m_node->m_mesh.origin, m_moveDelta, color, 0.75f, 0.01f );

}

void CWallExtrudeAction::Act()
{
	auto node = CreateExtrusion();
	auto pNode = node.get();
	GetWorldEditor().RegisterNode(std::move(node));
	m_quad = pNode;
	pNode->ConnectTo(m_node);
	pNode->Update();
}

void CWallExtrudeAction::Undo()
{
	if (m_quad.IsValid())
	{
		GetWorldEditor().DeleteNode(m_quad.Node());
		m_node->DisconnectFrom(m_quad);
		m_node->UpdateThisOnly();
	}

}

void CWallExtrudeAction::Redo()
{
	SASSERT(!m_quad.Node());

	auto node = CreateExtrusion();
	auto pNode = node.get();
	GetWorldEditor().AssignID(std::move(node), m_quad.ID());
	node->ConnectTo(m_node);
	node->Update();

}

int CVertDragAction::GetSelectionType()
{
	return ACT_SELECT_VERT;
}
