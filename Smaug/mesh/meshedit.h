#pragma once

#include "mesh.h"

// Creates a convex polygon with n sides and a normal of (0,1,0)
clasp_ref<meshPart_t> partFromPolygon(clasp_ref<mesh_t> mesh, int sides, glm::vec3 offset);

// Creates a quad that stems out of a edge.
clasp_ref<meshPart_t> pullQuadFromEdge(clasp_ref<vertex_t> stem, glm::vec3 offset);

// Creates quads from a list of stems. Useful for making cylinders out of faces.
std::vector<clasp_ref<vertex_t>> extrudeEdges(std::vector<clasp_ref<vertex_t>>& stems, glm::vec3 offset);

// Caps off a stem list with a mesh part. Useful for closing an extrusion.
clasp_ref<meshPart_t> endcapEdges(std::vector<clasp_ref<vertex_t>>& stems);

// Inverts the normal of a mesh part
void invertPartNormal(clasp_ref<meshPart_t> part);

// Pushes verts from a face into a vector. Vector is guaranteed to be ordered.
void vertexLoopToVector(clasp_ref<face_t> face, std::vector<clasp_ref<vertex_t>>& stems);
