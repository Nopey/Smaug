#pragma once
#include "mesh.h"


// unused
//clasp_ref<face_t> sliceMeshPartFace(clasp_ref<meshPart_t> mesh, std::vector<clasp<face_t>>& faceVec, clasp_ref<face_t> face, vertex_t& start, vertex_t& end);

// unused
// void triangluateMeshPartFaces(meshPart_t& mesh, std::vector<face_t*>& faceVec);
void triangluateMeshPartConvexFaces(clasp_ref<meshPart_t> mesh, std::vector<clasp<face_t>>& faceVec);
void convexifyMeshPartFaces(clasp_ref<meshPart_t> mesh, std::vector<clasp<face_t>>& faceVec);

void optimizeParallelEdges(clasp_ref<meshPart_t> part, std::vector<clasp<face_t>>& faceVec);