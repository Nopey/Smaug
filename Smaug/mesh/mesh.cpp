#include "mesh.h"
#include "raytest.h"
#include "containerutil.h"
#include "utils.h"
#include <glm/geometric.hpp>

// Gets the normal of the *next* vert
glm::vec3 vertNextNormal(vertex_t const &vert)
{
	vertex_t &between = vert.edge->vert;
	vertex_t &end = between.edge->vert;

	glm::vec3 edge1 = (*vert.vert) - (*between.vert);
	glm::vec3 edge2 = (*between.vert) - (*end.vert);
	
	return glm::cross(edge1, edge2);
}

// Returns the unnormalized normal of a face
glm::vec3 faceNormal(face_t const &face, glm::vec3 *outCenter )
{
	// Not fond of this
	// TODO: We should probably cache this data and/or add a quick route for triangulated faces!
	
	glm::vec3 center = faceCenter(face);

	if (outCenter)
		*outCenter = center;

	if (face.flags & FaceFlags::FF_CONVEX)
	{
		return convexFaceNormal(face);
	}

	glm::vec3 faceNormal = { 0,0,0 };

	vertex_t const &vs = *face.verts.front(), &l = vs, &v = l.edge->vert;
	do
	{
		faceNormal += glm::cross(center - (*v.vert), (*l.vert) - center);
	} while (&l != &vs);

	return faceNormal;
}

glm::vec3 convexFaceNormal(face_t const &face)
{
	return vertNextNormal(*face.verts.front());
}

/////////////////////
// Mesh management //
/////////////////////


// Adds vertices to a mesh for use in face definition
std::span<clasp<glm::vec3>> addMeshVerts(mesh_t& mesh, std::span<glm::vec3> points)
{
	size_t start = mesh.verts.size();
	for ( auto &point : points )
	{
		mesh.verts.push_back(make_clasp<glm::vec3>(point));
	}
	return std::span(mesh.verts).subspan(start);
}

// Creates and defines a new face within a mesh
static void defineFace(clasp_ref<face_t> face, std::span<clasp_ref<glm::vec3>> vecs);
clasp_ref<meshPart_t> addMeshFace(clasp_ref<mesh_t> mesh, std::span<clasp_ref<glm::vec3>> points)
{
	mesh->parts.push_back(make_clasp<meshPart_t>(mesh));
	auto mp = mesh->parts.back().borrow();

	defineFace(mp, points);

	return mp;
}


// Wires up all the HEs for this face
// Does not triangulate
static void defineFace(clasp_ref<face_t> face, std::span<clasp_ref<glm::vec3>> vecs)
{
	// Clear out our old data
	face->verts.clear();
	face->edges.clear();
	
	// Populate our face with HEs
	clasp_ref<vertex_t> lastVert;
	clasp_ref<halfEdge_t> lastHe;
	for (int i = 0; i < vecs.size(); i++)
	{
		// HE stems out of V

		face->edges.push_back(make_clasp<halfEdge_t>());
		clasp_ref<halfEdge_t> he = face->edges.back();

		he->face = face;
		he->flags |= EdgeFlags::EF_OUTER;

		face->verts.push_back(make_clasp<vertex_t>(vecs[i], he));
		clasp_ref<vertex_t> v = face->verts.back();

		// Should never be a situation where these both arent null or something
		// If one is null, something's extremely wrong
		if (lastHe && lastVert)
		{
			lastHe->next = he;
			lastHe->vert = v;
		}

		lastHe = he;
		lastVert = v;
	}

	// Link up our last HE
	if (lastHe && lastVert)
	{
		lastHe->next = face->edges.front();
		lastHe->vert = face->verts.front();
	}
}

void defineMeshPartFaces(clasp_ref<meshPart_t> mesh)
{
	// Clear out our old faces
	mesh->collision.clear();
	mesh->tris.clear();
	mesh->sliced = {};

	if (mesh->verts.size() == 0)
		return;

	// Create a face clone of the part
	mesh->collision.push_back(make_clasp<face_t>());
	auto f = mesh->collision.back().borrow();
	cloneFaceInto(mesh, f);
	f->parent = mesh;
	f->flags = FaceFlags::FF_NONE;
	
	// Mark our edges
	for (auto &e : f->edges)
		e->flags |= EdgeFlags::EF_PART_EDGE;

	// Compute our normal
	mesh->normal = glm::normalize(faceNormal(mesh));

}

void cloneFaceInto(face_t &in, clasp_ref<face_t> cloneOut)
{
	// Clear out our old data
	cloneOut->verts.clear();
	cloneOut->edges.clear();

	clasp_ref<vertex_t>   lastCloneVert;
	clasp_ref<halfEdge_t> lastCloneEdge;

	clasp_ref<vertex_t> vs = in.verts.front(), v = vs;
	do
	{
		// Clone this vert and the edge that stems out of it
		cloneOut->edges.push_back(make_clasp<halfEdge_t>());
		clasp_ref<halfEdge_t> ch = cloneOut->edges.back();
		ch->face = cloneOut;
		ch->flags = v->edge->flags;
		cloneOut->verts.push_back(make_clasp<vertex_t>(v->vert, ch));
		clasp_ref<vertex_t> cv = cloneOut->verts.back();
		
		if (lastCloneVert)
		{
			lastCloneEdge->next = ch;
			lastCloneEdge->vert = cv;
		}


		lastCloneVert = cv;
		lastCloneEdge = ch;

		v = v->edge->vert;
	} while (v != vs);

	if (lastCloneEdge)
	{
		lastCloneEdge->next = cloneOut->edges.front();
		lastCloneEdge->vert = cloneOut->verts.front();
	}

	cloneOut->flags = in.flags;
	cloneOut->parent = in.parent;
}

aabb_t addPointToAABB(aabb_t aabb, glm::vec3 point)
{
	if (point.x > aabb.max.x)
		aabb.max.x = point.x;
	if (point.y > aabb.max.y)
		aabb.max.y = point.y;
	if (point.z > aabb.max.z)
		aabb.max.z = point.z;

	if (point.x < aabb.min.x)
		aabb.min.x = point.x;
	if (point.y < aabb.min.y)
		aabb.min.y = point.y;
	if (point.z < aabb.min.z)
		aabb.min.z = point.z;

	return aabb;
}


unsigned int edgeLoopCount(vertex_t& sv)
{
	unsigned int i = 0;
	vertex_t& v = sv;
	do
	{
		i++;
		v = *v.edge->vert;
	} while (&v != &sv);
	return i;
}

aabb_t meshAABB(mesh_t& mesh)
{

	glm::vec3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	glm::vec3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
	for (auto &v : mesh.verts)
	{
		if (v->x > max.x)
			max.x = v->x;
		if (v->y > max.y)
			max.y = v->y;
		if (v->z > max.z)
			max.z = v->z;

		if (v->x < min.x)
			min.x = v->x;
		if (v->y < min.y)
			min.y = v->y;
		if (v->z < min.z)
			min.z = v->z;
	}

	return { min,max };
}

void recenterMesh(mesh_t& mesh)
{
	// Recenter the origin
	glm::vec3 averageOrigin = glm::vec3(0, 0, 0);
	for(auto &v : mesh.verts)
	{
		averageOrigin += *v;
	}
	averageOrigin /= mesh.verts.size();

	// Shift the vertexes
	mesh.origin += averageOrigin;
	for (auto &v : mesh.verts)
	{
		*v -= averageOrigin;
	}
}

glm::vec3 faceCenter(face_t const &face)
{
	// Center of face
	glm::vec3 center = { 0,0,0 };
	for (auto const &v : face.verts)
		center += *v->vert;
	center /= face.verts.size();

	return center;
}

void settleHECustody(clasp_ref<face_t> face, clasp_ref<face_t> newFace)
{
	for (int i = 0; i < face->edges.size(); i++)
	{
		auto& he = face->edges[i];

		// This assertion is failing atm.. :(
		SASSERT(he && (he->face.get() == face.get() || he->face.get() == newFace.get()));

		if (he->face.get() == newFace.get())
		{
			// Move ownership of this halfEdge to the other vector
			// AjPau5QYtYs

			newFace->edges.push_back(std::move(he));
			// remove he from face->edges
			std::swap(he, face->edges.back());
			face->edges.pop_back();
			// keep iterator from misbehaving
			i--;
		}
	}
}


void settleVertCustody(clasp_ref<face_t> face, clasp_ref<face_t> newFace)
{
	for (int i = 0; i < face->verts.size(); i++)
	{
		auto& v = face->verts[i];

		// This assertion is failing atm.. :(
		SASSERT(v && v->edge && (v->edge->face.get() == face.get() || v->edge->face.get() == newFace.get()));

		if (v->edge->face.get() == newFace.get())
		{
			// Move ownership of this vert to the other vector

			newFace->verts.push_back(std::move(v));
			// remove he from face->edges
			std::swap(v, face->verts.back());
			face->verts.pop_back();
			// keep iterator from misbehaving
			i--;
		}
	}
}

void graphFace(clasp_ref<face_t> tasty, char const *filename_postfix)
{
	char filename[256];
	sprintf_s(filename, "out_%p%s.dot", tasty.get(), filename_postfix);

	FILE* fout;
	SASSERT(!fopen_s(&fout, filename, "w"));
	if (!fout) return;
	fprintf(fout, "digraph face {\n");

	// Edges
	fprintf(fout, "\tsubgraph edges {\n");
	fprintf(fout, "\t\tlabel = \"Edges\";\n");
	fprintf(fout, "\t\tnode [color = blue];\n");
	for (auto& e : tasty->edges)
	{
		fprintf(fout, "\t\t\"edge%p\";\n", e.get());
	}
	fprintf(fout, "\t}\n");

	// Verts
	fprintf(fout, "\tsubgraph verts {\n");
	fprintf(fout, "\t\tlabel = \"Verts\";\n");
	fprintf(fout, "\t\tcolor = red;\n");
	fprintf(fout, "\t\tnode [color = red];\n");
	for (auto& v : tasty->verts)
	{
		fprintf(fout, "\t\t\"vert%p\";\n", v.get());
	}
	fprintf(fout, "\t}\n");

	// Edge->Edge relations
	fprintf(fout, "\n");
	for (auto& e : tasty->edges)
	{
		fprintf(fout, "\t\"edge%p\" -> \"edge%p\";\n", e.get(), e->next.get());
	}

	// Edge->Vert relations
	fprintf(fout, "\n");
	for (auto& e : tasty->edges)
	{
		fprintf(fout, "\t\"edge%p\" -> \"vert%p\";\n", e.get(), e->vert.get());
	}

	// Vert->Edge relations
	fprintf(fout, "\n");
	for (auto& v : tasty->verts)
	{
		fprintf(fout, "\t\"vert%p\" -> \"edge%p\";\n", v.get(), v->edge.get());
	}

	fprintf(fout, "}\n");
	fclose(fout);
}

template<>
void breakClaspCycles(face_t& f)
{
	breakClaspCycles(f.edges);
	breakClaspCycles(f.parent);
}

template<>
void breakClaspCycles(meshPart_t& p)
{
	breakClaspCycles(p.tris);
	breakClaspCycles(p.collision);
	if (p.sliced)
		breakClaspCycles(p.sliced);
	breakClaspCycles((face_t&)p);
}

template<>
void breakClaspCycles(halfEdge_t& he)
{
	breakClaspCycles(he.face);
	breakClaspCycles(he.vert);
	breakClaspCycles(he.next);
	breakClaspCycles(he.pair);
}

template<>
void breakClaspCycles(vertex_t& v)
{
	breakClaspCycles(v.edge);
	breakClaspCycles(v.vert);
}

template<>
void breakClaspCycles(cuttableMesh_t& c)
{
	breakClaspCycles( (face_t&)c );
}
