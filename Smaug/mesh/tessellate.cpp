#include "tessellate.h"
#include "raytest.h"
#include "meshtest.h"
#include "log.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <debugbreak.h>

///////////////////////
// Mesh face slicing //
///////////////////////

// For use where we know which side's going to end up larger
// Returns the new face

static clasp_ref<face_t> sliceMeshPartFaceUnsafe(clasp_ref<meshPart_t> mesh, std::vector<clasp<face_t>>& faceVec, clasp_ref<face_t> face, vertex_t &start, vertex_t &end)
{
	SASSERT(&start != &end);

	// Create new face, eventually over verts [end, start]
	// (face will become [start, end])
	faceVec.push_back(make_clasp<face_t>());
	clasp_ref<face_t> newFace = faceVec.back();	
	newFace->parent = mesh;
	newFace->flags = face->flags;

	// The halfedges leading out of the vertices
	clasp_ref<halfEdge_t> heStart = start.edge, heEnd = end.edge;

	// newFace receives ownership of original vertices (end, start]
	// and original halfEdges (heEnd, heStart]
	for (clasp_ref<halfEdge_t> he = heEnd; he != heStart;)
	{
		he = he->next;
		he->face = newFace;
	}
	settleHECustody(face, newFace);
	settleVertCustody(face, newFace);

	// Create copy of heEnd for newFace
	newFace->edges.push_back(make_clasp<halfEdge_t>(heEnd->vert, nullptr, newFace, heEnd->next));
	// owned by newFace
	auto newHeEnd = newFace->edges.back().borrow();
	// ---- vice versa
	face->edges.push_back(make_clasp<halfEdge_t>(heStart->vert, nullptr, face, heStart->next));
	// owned by face
	auto newHeStart = face->edges.back().borrow();

	// Create end vertex for newFace, with newHeEnd
	newFace->verts.push_back(make_clasp<vertex_t>(end.vert, newHeEnd));
	// newFace's copy of the end vert
	auto newEnd = newFace->verts.back().borrow();
	// ---- vice versa
	face->verts.push_back(make_clasp<vertex_t>(start.vert, newHeStart));
	// face's copy of the start vert
	auto newStart = face->verts.back().borrow();

	// heStart (owned by newFace) gets repurposed as the diagonal
	heStart->vert = newEnd;
	heStart->next = newHeEnd;
	// ---- vice versa
	heEnd->vert = newStart;
	heEnd->next = newHeStart;

	return newFace;
}

#if 0
// This will subdivide a mesh's face from start to end
// It will return the new face
// This cut will only occur on ONE face
clasp_ref<face_t> sliceMeshPartFace(clasp_ref<meshPart_t> mesh, std::vector<clasp<face_t>>& faceVec, clasp_ref<face_t> face, vertex_t& start, vertex_t& end)
{
	// No slice
	if (&start == &end)
		return face;

	// Don't like this much...
	int between = 0;
	for(vertex_t *vert = &start; vert != &end; vert = vert->edge.next->get() )
		between++;

	// Start should always be the clockwise start of the smaller part
	if (between < face->edges.size() / 2.0f)
	{
		return sliceMeshPartFaceUnsafe(mesh, faceVec, face, end, start);
	}

	// Now that we know which side's larger, we can safely slice
	return sliceMeshPartFaceUnsafe(mesh, faceVec, face, start, end);
}
#endif

static void deleteVertFromEdge(halfEdge_t &edge)
{
	face_t &face = edge.face.deref();

	{
		auto found = std::find(face.verts.begin(), face.verts.end(), edge.vert);

		SASSERT(found != std::end(face.verts));

		// Move vertex we plan on deleting to the back
		std::swap(face.verts.back(), *found);
	}

	// clear out our reference to the vert
	// NOTE: Use edge->next->vert instead if we remove the optional marking from this field
	edge.vert = {};

	// destroy the vert
	face.verts.pop_back();
}

static void deleteEdgeButNotItsVert( clasp_ref<halfEdge_t> &edge )
{
	if(!edge)
		return;

	face_t& face = edge->face.deref();

	{
		auto found = std::find(face.edges.begin(), face.edges.end(), edge);

		SASSERT(found != std::end(face.edges));

		// Move edge we plan on deleting to the back
		std::swap(face.edges.back(), *found);
	}


	// clear out our reference to the edge
	// NOTE: Use edge->next instead if we remove the optional marking from this field
	edge = {};

	// destroy the edge
	face.edges.pop_back();
}


// This will REBUILD THE FACE!
// The next TWO edges from stem will be deleted!
static clasp_ref<halfEdge_t> fuseEdges(face_t &face, vertex_t &stem)
{
	// Fuse stem and it's next edge together
	// Hate this. Make it better somehow...

	// This will definitely fall apart with less than 3.
	SASSERT(face.edges.size() >= 3);

	// Since something's already pointing to stem->edge, it's going to take the place of post->edge
	clasp_ref<halfEdge_t> replacer = stem.edge;
	
	// These two are optional so we can delete them partway through the function
	// Edge immediately before the fuse
	clasp_ref<halfEdge_t> before = replacer->next;
	// Edge immediately after the fuse
	clasp_ref<halfEdge_t> post = before->next;

	// remove replacer->vert
	deleteVertFromEdge(*replacer);

	// Are we perfectly stuck together?
	if (closeTo(glm::distance(*before->vert->vert, *stem.vert), 0))
	{
		// Link it
		replacer->vert = post->vert;

		replacer->next = post->next;

		// Delete the edges first, as they refer to the verts
		deleteEdgeButNotItsVert(post);

		// Clear out stuff to be fused
		deleteVertFromEdge(*before);
	}
	else
	{
		// Uneven fuse. We need to retain next->vert
		// Link it
		replacer->vert = before->vert;
		
		replacer->next = post;
	}

	deleteEdgeButNotItsVert(before);

	return replacer;
}


/////////////////////////////
// Mesh face triangulation //
/////////////////////////////

#if 0 // unused <3
// Clean this all up!!
// Takes in a mesh part and triangulates every face within the part
// TODO: This might need a check for parallel lines!
void triangluateMeshPartFaces(meshPart_t& mesh, std::vector<face_t*>& faceVec)
{
	// As we'll be walking this, we wont want to walk over our newly created faces
	// Store our len so we only get to the end of the predefined faces
	size_t len = faceVec.size();
	for (size_t i = 0; i < len; i++)
	{
		face_t* face = faceVec[i];
		if (!face)
			continue;
		
		// Discard Tris
		if (face->edges.size() < 4)
			continue;

		
		// Get the norm of the face for later testing 
		glm::vec3 faceNorm = faceNormal(face);
		
		//int nU, nV;
		//findDominantAxis(faceNorm, nU, nV);

		// On odd numbers, we use start + 1, end instead of start, end - 1
		// Makes it look a bit like we're fitting quads instead of tris
		int alternate = 0;


		// In these loop, we'll progressively push the original face to become smaller and smaller
		while (face->verts.size() > 3)
		{
			// Vert 0 will become our anchor for all new faces to connect to
			vertex_t* end = face->verts[face->verts.size() - 1 - fmod(alternate, 2)];
			vertex_t* v0 = end->edge->next->vert;


			int attempts = 0;
			bool shift;
			do{
				shift = false;

				// Will this be concave?
				vertex_t* between = end->edge->vert;
				glm::vec3 edge1 = (*end->vert) - (*between->vert);
				glm::vec3 edge2 = (*between->vert) - (*v0->vert);
				glm::vec3 triNormal = glm::cross(edge1, edge2);

				//SASSERT(triNormal.x != 0 || triNormal.y != 0 || triNormal.z != 0);

				// If we get a zero area tri, WHICH WE SHOULD NOT PASS IN OR CREATE, trim it off
				if (triNormal.x == 0 && triNormal.y == 0 && triNormal.z == 0)
				{
					//SASSERT(0);
					Log::Warn("Zero area triangle :tired_face:");

					// Fuse v0 and end together 
					fuseEdges(face, end);

					goto escapeTri;
				}

				float dot = glm::dot(triNormal, faceNorm);
				if (dot < 0)
					shift = true;
				else
				{
					// Dot was fine. Let's see if anything's in our new tri

					//glm::vec3 u = tritod(*end->vert, *between->vert, *v0->vert, nU);
					//glm::vec3 v = tritod(*end->vert, *between->vert, *v0->vert, nV);

					// I fear the cache misses...
					for (vertex_t* c = v0->edge->vert; c != end; c = c->edge->vert)
					{
						glm::vec3 ptt = *c->vert;
						if (testPointInTriNoEdges(ptt, *end->vert, *between->vert, *v0->vert))//(ptt[nU], ptt[nV], u, v))
						{
							shift = true;
							break;
						}
					}
				}

				if (shift)
				{
					// Shift to somewhere it shouldn't be concave
					v0 = v0->edge->vert;
					end = end->edge->vert;
					attempts++;
				}
			}
			while (shift && attempts < 10);

			// Wouldn't it just be better to implement a quick version for triangulate? We're doing a lot of slices? Maybe just a bulk slicer?
			sliceMeshPartFaceUnsafe(mesh, faceVec, face, v0, end);

			escapeTri:
			alternate++;
		};

		
	}

}
#endif

// Not for use on concaves!
void triangluateMeshPartConvexFaces(clasp_ref<meshPart_t> mesh, std::vector<clasp<face_t>>& faceVec)
{
	// prevent reallocation during below loop, trying to prevent explosions
	faceVec.reserve(faceVec.size()*2);

	// As we'll be walking this, we wont want to walk over our newly created faces
	// Store our len so we only get to the end of the predefined faces
	size_t len = faceVec.size();
	Log::Print("Len = %zd\n", len);
	for (size_t i = 0; i < len; i++)
	{
		auto &face = faceVec[i];

		// Discard Tris
		if (face->edges.size() < 4)
			continue;

		// On odd numbers, we use start + 1, end instead of start, end - 1
		// Makes it look a bit like we're fitting quads instead of tris
		int alternate = 0;

		// In these loop, we'll progressively push the original face to become smaller and smaller
		while (face->verts.size() > 3)
		{
			// Vert 0 will become our anchor for all new faces to connect to
			auto end = face->verts[face->verts.size() - 1 - fmod(alternate, 2)].borrow();
			auto v0 = end->edge->next->vert;

			// Wouldn't it just be better to implement a quick version for triangulate? We're doing a lot of slices? Maybe just a bulk slicer?
			auto newFace = sliceMeshPartFaceUnsafe(mesh, faceVec, face, *v0, *end);
			SASSERT(newFace->verts.size() == 3);

			alternate++;
		};
	}
	Log::Print("Successfully triangulated one meshPart's convex faces..\n");
}

// This function fits convex faces to the concave face
// It might be slow, bench it later
void convexifyMeshPartFaces(clasp_ref<meshPart_t> mesh, std::vector<clasp<face_t>>& faceVec)
{
	
	clasp<halfEdge_t> gapFiller = make_clasp<halfEdge_t>();
	
	// Get the norm of the face for later testing 
	glm::vec3 faceNorm = mesh->normal;//faceNormal(&mesh);
	
	size_t len = faceVec.size();
	for (size_t i = 0; i < len; i++)
	{
		auto face = faceVec[i].borrow();

		// Get the norm of the face for later testing 
		//glm::vec3 faceNorm = faceNormal(face);

		int sanity = 0;


		// We only want to start cutting when our convexStart is a concave
		bool startSlicing = false;
		
		// We're not always going to be able to concave connect...
		// But it's at least worth trying to the first time around?
		// TODO: Check!
		bool forgetConcConnect = false;
	startOfLoop:
		// Discard Tris, they're already convex
		if (face->edges.size() < 4)
			continue;

		clasp_ref<vertex_t> vStart = face->verts.front();
		clasp_ref<vertex_t> convexStart = vStart;
		clasp_ref<vertex_t> vert = vStart;
		sanity = 0;
		startSlicing = forgetConcConnect;
		do
		{
			clasp_ref<vertex_t> between = vert->edge->vert;
			clasp_ref<vertex_t> end = between->edge->vert;

			glm::vec3 edge1 = (*vert->vert) - (*between->vert);
			glm::vec3 edge2 = (*between->vert) - (*end->vert);
			glm::vec3 triNormal = glm::cross(edge1, edge2);
			float triDot = glm::dot(edge1, edge2);


			if (triNormal.x == 0 && triNormal.y == 0 && triNormal.z == 0 && triDot < 0)
			{
				//printf("%f dot\n", triDot);
				//DebugDraw().HEFace(face, randColorHue(), 0.25, 1000);
				Log::Warn("We got a zero area tri! Yuck!!");
				//SASSERT(0);
				fuseEdges(face, vert);
				// Fusing induces a change in edge count. Go back to the top 
				goto startOfLoop;
			}


			bool concave = false;
			float dot = glm::dot(triNormal, faceNorm);

			if (dot < 0)
			{
				// Uh oh! This'll make us concave!
				concave = true;
			}
			
			
			if (startSlicing)
			{
				//if (vert != convexStart)
				{
					if (!concave)
					{

						// If we're working out a slice, we need to make sure the slice is convex

						// Would trying to connect up to convex start make us concave?
						glm::vec3 bridge1 = (*end->vert) - (*convexStart->vert);
						glm::vec3 bridge2 = (*convexStart->vert) - (*convexStart->edge->vert->vert);
						glm::vec3 bridgeNormal = glm::cross(bridge1, bridge2);
						float bridgeDot = glm::dot(bridgeNormal, faceNorm);

						if (bridgeDot < 0)
							concave = true;
						else
						{
							bridge1 = (*between->vert) - (*end->vert);
							bridge2 = (*end->vert) - (*convexStart->vert);
							bridgeNormal = glm::cross(bridge1, bridge2);
							bridgeDot = glm::dot(bridgeNormal, faceNorm);

							if (bridgeDot < 0)
								concave = true;
						}
					}
				}
				if (!concave)
				{
					// Hmm... Doesn't seem concave... Let's double check
					// Patch up the mesh and perform a point test

					// Shouldnt need to store between's edge's next...
					clasp_ref<halfEdge_t> tempHE = end->edge;

					// Temp patch it
					clasp_ref<halfEdge_t> cse = convexStart->edge;
					gapFiller->next = cse;
					gapFiller->vert = convexStart;

					end->edge = gapFiller;
					between->edge->next = gapFiller;

					// Loop over our remaining verts and check if in our convex fit
					for (clasp_ref<vertex_t> ooc = tempHE->vert; ooc != convexStart; ooc = ooc->edge->vert)
					{
						if (pointInConvexLoopNoEdges(convexStart, *ooc->vert))
						{
							// Uh oh concave!
							concave = true;
							break;
						}
					}

					// Undo our patch
					end->edge = tempHE;
					between->edge->next = tempHE;
				}
			}

			if (concave)
			{
				// This shape's concave!
				
				if (startSlicing && convexStart != vert)
				{
					// We'll need to cap off this shape at whatever we had last time...
					// Connect between and start
					sliceMeshPartFaceUnsafe(mesh, faceVec, face, between, convexStart);
					goto startOfLoop;
				}
				else
				{
					startSlicing = true;
					// Shift forwards and try that one later
					convexStart = between;// convexStart->edge->vert;
					sanity++;
				}
			}

			vert = between;
		} while ((startSlicing ? vert->edge->vert : vert) /*->edge->vert*/ != convexStart
		&& sanity <= face->verts.size());

		if (sanity >= face->verts.size() && !forgetConcConnect )
		{
			// If we get to this point, one of two things have happened
			// 1) The mesh is terrible
			// 2) We could not make any direct concave to concave connections
			// Let's assume 2 and try again...
			forgetConcConnect = true;
			goto startOfLoop;
		}
		SASSERT_S(!startSlicing);
		// I mean, if every vertex is concave, I guess it's convex? Right? Backwards normal?		
		//if (sanity >= face->verts.size())
		//	printf("sanity check failure\n");
		SASSERT_S(sanity < face->verts.size());
}


	// Mark our creation as convex
	for (auto &f : faceVec)
		f->flags |= FaceFlags::FF_CONVEX;

}

void optimizeParallelEdges(clasp_ref<meshPart_t> part, std::vector<clasp<face_t>>& faceVec)
{
	for( auto &face : faceVec)
	{
	startOfLoop:
		// Discard Tris, they're already perfect :flushed:
		if (face->edges.size() < 4)
			continue;

		clasp_ref<vertex_t> vStart = face->verts.front();
		clasp_ref<vertex_t> vert = vStart;

		do
		{
			clasp_ref<vertex_t> between = vert->edge->vert;
			clasp_ref<vertex_t> end = between->edge->vert;

			glm::vec3 edge1 = (*vert->vert) - (*between->vert);
			glm::vec3 edge2 = (*between->vert) - (*end->vert);
			glm::vec3 triNormal = glm::cross(edge1, edge2);

			if (triNormal.x == 0 && triNormal.y == 0 && triNormal.z == 0)
			{
				fuseEdges(face, vert);
				goto startOfLoop;
			}

			vert = between;
		} while (vert->edge->vert != vStart);
	}

}
