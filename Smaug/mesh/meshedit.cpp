#include "meshedit.h"


clasp_ref<meshPart_t> partFromPolygon(clasp_ref<mesh_t> mesh, int sides, glm::vec3 offset)
{
	std::vector<glm::vec3> points;
	for (int i = 0; i < sides; i++)
		points.push_back(glm::vec3(-cos(i * (PI * 2.0f) / sides), 0, sin(i * (PI * 2.0f) / sides)) + offset);

	auto p = addMeshVerts(*mesh, points);

	// TODO: Magnus: Ugly re-vectoring
	std::vector<clasp_ref<glm::vec3>> p2(p.begin(), p.end());

	return addMeshFace(mesh, p2);
}

clasp_ref<meshPart_t> pullQuadFromEdge(clasp_ref<vertex_t> stem, glm::vec3 offset)
{
	clasp_ref<mesh_t> parent = parentMesh(stem->edge->face);
	glm::vec3 verts[2] = { *stem->vert + offset, *stem->edge->vert->vert + offset };
	auto p = addMeshVerts(parent, verts);
	clasp_ref<glm::vec3> v[4] = { p[0], p[1], stem->edge->vert->vert, stem->vert };
	return addMeshFace(parent, v);
}

std::vector<clasp_ref<vertex_t>> extrudeEdges(std::vector<clasp_ref<vertex_t>>& stems, glm::vec3 offset)
{
	if (stems.size() < 2)
		return {};

	clasp_ref<mesh_t> parent = parentMesh(stems.front()->edge->face);

	std::vector<glm::vec3> verts;
	verts.reserve(stems.size());
	for (auto &s : stems)
		verts.push_back(*s->vert + offset);
	auto p = addMeshVerts(parent, verts);

	std::vector<clasp_ref<vertex_t>> newStems;
	for (int i = 0; auto &s : stems)
	{
		clasp_ref<glm::vec3> v[4] = { p[i], p[(i + 1) % stems.size()], s->edge->vert->vert, s->vert };
		newStems.push_back(addMeshFace(parent, v)->verts[0]);
		i++;
	}
	return newStems;
}


clasp_ref<meshPart_t> endcapEdges(std::vector<clasp_ref<vertex_t>>& stems)
{
	if (stems.size() < 2)
		return {};

	clasp_ref<mesh_t> parent = parentMesh(stems.front()->edge->face);

	std::vector<clasp_ref<glm::vec3>> points;
	for (int i = stems.size() - 1; i >= 0; i--)
		points.push_back(stems[i]->vert);

	return addMeshFace(parent, points);
}

void invertPartNormal(clasp_ref<meshPart_t> part)
{
	if (part->verts.size() < 2 || part->edges.size() < 2)
		return;

	// So that we maintain pairs and etc, we actually have to relink this mesh, not just remake it but backwards

	clasp_ref<vertex_t> vs = part->verts.front(), last = vs;
	clasp_ref<halfEdge_t> lastEdge = last->edge, firstEdge = lastEdge;

	// Shift everything forward
	last = last->edge->vert;
	vs = last;
	clasp_ref<vertex_t> v = last->edge->vert;

	do
	{
		clasp_ref<halfEdge_t> ce = last->edge;
		ce->vert = last;
		ce->next = lastEdge;
		last->edge = lastEdge;
		lastEdge = ce;

		last = v;
		v = v->edge->vert;
	} while (last != vs);

}


void vertexLoopToVector(clasp_ref<face_t> face, std::vector<clasp_ref<vertex_t>>& stems)
{
	clasp_ref<vertex_t> vs = face->verts.front(), v = vs;
	do
	{
		stems.push_back(v);
		v = v->edge->vert;
	} while (v != vs);
}
