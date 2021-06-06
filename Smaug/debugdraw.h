#pragma once

#include "mesh.h"

#include <glm/vec3.hpp>
#include <vector>
#include <memory>

class CDebugDraw
{
public:
	class ITempItem
	{
	public:
		virtual void Draw() = 0;
		virtual bool Dead(float curTime) = 0;
	};

	void Line(glm::vec3 start, glm::vec3 end, glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f);
	void Line(vertex_t const& start, vertex_t* end, glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f) { Line(*start.vert + parentMesh(start.edge->face)->origin, *end->vert + parentMesh(end->edge->face)->origin, color, width, decay); }
	
	void LineDelta(glm::vec3 start, glm::vec3 delta, glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f) { Line(start, start + delta, color, width, decay); }
	void LineDelta(vertex_t const& start, glm::vec3 delta, glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f) { LineDelta(*start.vert + parentMesh(start.edge->face)->origin, delta, color, width, decay); }
	
	void HELoop(vertex_t const& start, vertex_t const& end, glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f);
	void HELoop(vertex_t const& vert,   glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f) { HELoop(vert, vert, color, width, decay); }
	void HELoop(halfEdge_t const& he,   glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f) { HELoop(he.vert, color, width, decay); }
	void HEFace(clasp_ref<face_t> face,     glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f);
	void HEPart(meshPart_t const& part, glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f);

	void AABB(aabb_t aabb, glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f);
	void AABB(glm::vec3 origin, aabb_t aabb, glm::vec3 color = { 1.0f,0.0f,1.0f }, float width = 1.0f, float decay = 5.0f);

	//	void Cube();
	void Draw();
private:
#ifndef NDEBUG
	std::vector<std::unique_ptr<ITempItem>> m_itemsToDraw;
#endif
};


CDebugDraw& DebugDraw();