#include "slice.h"
#include "mesh.h"
#include "meshtest.h"
#include "raytest.h"
#include "utils.h"
#include "tessellate.h"
#include <glm/geometric.hpp>

/////////////////////////////
// Mesh face shape cutting //
/////////////////////////////

// Cuts another face into a face using the cracking method
//
// Input
//  ________
// |        |
// |        |           /\
// |        |    +     /  \
// |        |         /____\
// |________|
//
// Output
//  ________
// |        |
// |   /\   |
// |  /  \  |
// | /____\ |
// |_______\|  < outer face is "cracked" and shaped around the cut
//               ready to be triangulated for drawing
//

// Mesh part must belong to a cuttable mesh!
// Cutter must have an opposing normal, verts are counter-clockwise
void opposingFaceCrack(cuttableMesh_t& mesh, meshPart_t* part, meshPart_t* cutter, face_t* targetFace)
{
	// This fails if either parts have < 3 verts
	if (part->verts.size() < 3 || cutter->verts.size() < 3)
		return;
	std::vector<vertex_t*>& cutterVerts = cutter->verts;

	// Find the closest two points
	float distClosest = FLT_MAX;
	vertex_t* v1Closest = nullptr;
	vertex_t* v2Closest = nullptr;
	int v2Index = 0;

	glm::vec3 cutterToLocal = +cutter->mesh->origin - mesh.origin;
	
	// We don't want to break this face's self representation
	// We're going to be editing child face #0
	// This means cracking *must* be done before triangulation
	// Which really sucks cause point testing is going to be waaay harder
	// We might want a representation below the mesh face for this...
	face_t* target = targetFace;// part->sliced->collision[0];
	
	// This might have horrible performance...
	for (auto v1 : target->verts)
	{
		// Skip out of existing cracks
		if (v1->edge->pair != nullptr)
			continue;

		int v2i = 0;
		for (auto v2 : cutterVerts)
		{
			glm::vec3 delta = *v1->vert - (*v2->vert + cutterToLocal);

			// Cheap dist
			// We don't need sqrt for just comparisons
			float dist = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
			if (dist < distClosest)
			{
				distClosest = dist;
				v1Closest = v1;
				v2Closest = v2;
				v2Index = v2i;
			}
			v2i++;
		}
	}


	// Let's start by tracking all of our new found points
	// aaand transforming them into our local space
	int startSize = mesh.cutVerts.size();
	for (auto v : cutterVerts)
	{
		glm::vec3* vec = new glm::vec3();
		*vec = *v->vert + cutterToLocal;
		mesh.cutVerts.push_back(vec);
		part->sliced->cutVerts.push_back(vec);
	}
	glm::vec3** cutVerts = mesh.cutVerts.data() + startSize;
	//long long transformPointerToLocal = cutterVerts - cutter->verts.data();

	// Since we checked the len earlier, v1 and v2 should exist...
	
	// Start the crack into the face
	halfEdge_t* crackIn = new halfEdge_t{nullptr, nullptr, target, nullptr};
	target->edges.push_back(crackIn);
	crackIn->flags |= EdgeFlags::EF_SLICED;

	vertex_t* curV = new vertex_t{cutVerts[v2Index]};
	crackIn->vert = curV;
	target->verts.push_back(curV);

	// Now we need to recreate our cutting mesh within this mesh
	halfEdge_t* curHE = crackIn;

	halfEdge_t* v2Edge = v2Closest->edge;
	
	halfEdge_t* otherHE = v2Edge;
	do
	{
		halfEdge_t* nHE = new halfEdge_t{};
		nHE->face = target;
		target->edges.push_back(nHE);

		curHE->next = nHE;
		curV->edge = nHE;

		// Don't like this!
		int offset = std::find(cutterVerts.begin(), cutterVerts.end(), otherHE->vert) - cutterVerts.begin();

		curV = new vertex_t{ cutVerts[offset] };
		nHE->vert = curV;
		target->verts.push_back(curV);

		curHE = nHE;

		otherHE = otherHE->next;
	} while (otherHE != v2Edge);


	

	// We need to crack v1 and v2 into two verts
	vertex_t* v1Out = new vertex_t{ v1Closest->vert, v1Closest->edge };
	target->verts.push_back(v1Out);

	// v1's going to need a new HE that points to v2
	halfEdge_t* crackOut = new halfEdge_t{ v1Out, crackIn, target, v1Out->edge };
	crackOut->flags |= EdgeFlags::EF_SLICED;
	curHE->next = crackOut;
	curV->edge = crackOut;
	crackIn->pair = crackOut;
	target->edges.push_back(crackOut);

	// Don't like this
	// Walk and find what leads up to v1Closest
	halfEdge_t* preHE;
	for (preHE = v1Closest->edge; preHE->vert != v1Closest; preHE = preHE->next);
	preHE->next = crackIn;
	v1Closest->edge = crackIn;
}


struct snipIntersect_t
{
	bool entering;
	float cutterT;
	glm::vec3 intersect;
	vertex_t* vert;
	halfEdge_t* edge;
};

struct inCutFace_t : public face_t
{
	int cullDepth = 0; // How many times has this been marked for culling?
};

// Pair of edges that can be dragged from the inner vert
// Could just deref out of right I guess...
struct draggable_t
{
	halfEdge_t* into;  // leads into vert
	vertex_t* vert;    // leads into left
	halfEdge_t* outof; // leads out of vert
};

// Intersect must exist within cut points!
draggable_t splitHalfEdgeAtPoint(halfEdge_t* he, glm::vec3* intersect)
{
	halfEdge_t* splitEdge = new halfEdge_t;
	vertex_t* splitVert = new vertex_t;

	// Split stems out of the vert
	splitVert->edge = splitEdge;
	splitVert->vert = intersect;

	// Split comes after original
	splitEdge->next = he->next;
	splitEdge->vert = he->vert;

	he->next = splitEdge;
	// splitVert stems OUT of he
	he->vert = splitVert;

	// Share the pair for now
	splitEdge->pair = he->pair;

	// Register up the parts
	face_t* face = he->face;
	face->edges.push_back(splitEdge);
	face->verts.push_back(splitVert);
	splitEdge->face = face;

	return { he, splitVert, splitEdge };
}

// Intersect must exist within cut points!
// Splits the other side too!
// Returns the new edge split off of the input
halfEdge_t* splitEdgeAtPoint(halfEdge_t* he, glm::vec3* intersect)
{
	halfEdge_t* pair = he->pair;

	if (pair)
	{
		// We need to alternate the pairs!
		//
		// O--.-->
		//    X      swap the pairs
		// <--.--O

		halfEdge_t* splitOrig = splitHalfEdgeAtPoint(he, intersect).outof;
		halfEdge_t* splitPair = splitHalfEdgeAtPoint(pair, intersect).outof;

		pair->pair = splitOrig;
		he->pair = splitPair;

		return splitOrig;
	}
	else
	{
		return splitHalfEdgeAtPoint(he, intersect).outof;

	}


}

halfEdge_t* addEdge(face_t* face)
{
	halfEdge_t* edge = new halfEdge_t;
	edge->face = face;
	face->edges.push_back(edge);
	return edge;
}

// Cracks a vertex in two and returns the opposing vert
// New vertex is floating! Requires linking to the list!
vertex_t* splitVertex(vertex_t* in, face_t* face)
{
	vertex_t* vert = new vertex_t;

	vert->edge = in->edge;
	vert->vert = in->vert;

	face->verts.push_back(vert);

	return vert;
}



// Takes in a split from splitHalfEdgeAtPoint
// Drags the split out to the selected position
//
//     | ^
//     | |
//  L  v |  R
//  <---.<---
//
// R is the original after a split and L is the new
void dragEdgeInto(draggable_t draggable, draggable_t target)
{
	halfEdge_t* edgeLeft = draggable.outof;
	halfEdge_t* edgeRight = draggable.into;
	vertex_t* vert = draggable.vert;

	face_t* face = edgeLeft->face;

	// Crack the target so intoTarget can stick into it
	// intoVert will stick out of target
	vertex_t* splitTarget = splitVertex(target.vert, face);
	// Crack the vert so intoTarget can stick out of it
	// intoVert will stick into vert
	vertex_t* splitVert = splitVertex(vert, face);
	
	// Points out of the right hand split
	halfEdge_t* intoVert = addEdge(face);
	// Points in to the left hand split
	halfEdge_t* intoTarget = addEdge(face);

	// Link the pairs as this is a full edge
	intoVert->pair = intoTarget;
	intoTarget->pair = intoVert;

	// Mark as sliced
	intoVert->flags   |= EdgeFlags::EF_SLICED;
	intoTarget->flags |= EdgeFlags::EF_SLICED;

	// Link up intoVert
	target.into->next = intoVert;
	target.vert->edge = intoVert;
	intoVert->vert = vert;
	intoVert->next = edgeLeft;

	// Link up intoTarget
	edgeRight->next = intoTarget;
	edgeRight->vert = splitVert;
	splitVert->edge = intoTarget;
	intoTarget->vert = splitTarget;
	intoTarget->next = splitTarget->edge;

}
draggable_t dragEdge(draggable_t draggable, glm::vec3* point)
{
	halfEdge_t* edgeLeft = draggable.outof;
	halfEdge_t* edgeRight = draggable.into;
	vertex_t* vert = draggable.vert;

	face_t* face = edgeLeft->face;

	// Crack the vert so edgeLeft can stick out of it
	vertex_t* splitVert = splitVertex(vert, face);

	// Points out of the right hand split
	halfEdge_t* out = addEdge(face);
	// Points in to the left hand split
	halfEdge_t* in = addEdge(face);
	
	// Link the pairs as this is a full edge
	in->pair = out;
	out->pair = in;

	// Mark as sliced
	in->flags  |= EdgeFlags::EF_SLICED;
	out->flags |= EdgeFlags::EF_SLICED;

	// In needs a vert to stem out of and for out to lead into 
	vertex_t* inStem = new vertex_t;
	inStem->edge = in;
	inStem->vert = point;
	face->verts.push_back(inStem);

	// Link up the right side into out
	// edgeRight's vert stays the same, but the vert now points into out
	vert->edge = out;
	edgeRight->next = out;

	// Link out into in
	out->vert = inStem;
	out->next = in;

	// Link up in into left
	in->next = edgeLeft;
	in->vert = splitVert;

	return { out, inStem, in };
}




// Roll around the cutter
// While rolling, check if any lines intersect
// If a line intersects, and we're leaving, stop adding data until we re-enter the face
// If a line intersects, and we're entering, keep adding data until we're out
// Line leaves when dot of cross of intersect and face norm is < 0
// TODO: Optimize this!
bool faceSnips(cuttableMesh_t& mesh, mesh_t& cuttingMesh, meshPart_t* part, meshPart_t* cutter, std::vector<face_t*>& cutFaces)
{
	glm::vec3 meshOrigin        = mesh.origin;
	glm::vec3 cuttingMeshOrigin = cuttingMesh.origin;
	glm::vec3 partNorm = part->normal;

	bool didCut = false;

	// Sanity checker
	int fullRollOver = 0;

	// Are we slicing into the face?
	bool cutting = false;

	// Last edit 
	draggable_t drag{ 0,0,0 };

	// We need to walk either into or out of the mesh
	// 

	bool invalidateSkipOver = false;

	// Loop the part
	// As we slice, more faces will be added. 
	// Those faces will get chopped after the first face is done
	vertex_t* cvStart = cutter->verts.front();
	vertex_t* cv = cvStart;
	do
	{
		// We need to store our intersections so we can sort by distance
		std::vector<snipIntersect_t> intersections;

		bool sharedLine = false;
		for (int i = 0; i < cutFaces.size(); i++)
		{
			face_t* face = cutFaces[i];

			vertex_t* pvStart = face->verts.front();
			vertex_t* pv = pvStart;
			do
			{
				// Dragged edges have pairs, normal cloned edges don't
				// TODO: improve determination of how to skip dragged edges
				if (pv->edge->flags & EdgeFlags::EF_SLICED)
				{
					pv = pv->edge->vert;
					continue;
				}

				// The line we're cutting
				glm::vec3 partStem = *pv->vert;
				glm::vec3 partEndLocal = *pv->edge->vert->vert;
				line_t pL = { partStem + meshOrigin, partEndLocal - partStem };

				// The line we're using to cut with
				glm::vec3 cutterStem = *cv->vert;
				glm::vec3 cutterEndLocal = *cv->edge->vert->vert;
				line_t cL = { cutterStem + cuttingMeshOrigin, cutterEndLocal - cutterStem };

				
				// If this line is parallel and overlapping with anything, it's not allowed to do intersections!
				glm::vec3 cross = glm::cross(cL.delta, pL.delta);
				if (!sharedLine && (cross.x == 0 && cross.y == 0 && cross.z == 0))
				{
					glm::vec3 absPartStem = partStem + meshOrigin;
					glm::vec3 absCutterStem = cutterStem + cuttingMeshOrigin;

					// If our lines are on top of eachother, we need to void some tests...
					glm::vec3 stemDir = glm::cross(cL.delta, absPartStem - absCutterStem);
					if (stemDir.x < 0.0001 && stemDir.y < 0.0001 && stemDir.z < 0.0001)
					{
						glm::vec3 absPartEnd = partEndLocal + meshOrigin;
						glm::vec3 absCutterEnd = cutterEndLocal + cuttingMeshOrigin;

						// Are we actually within this line though?
						/*
							end = stem + delta * m
							(e - s) . d = m;
						*/

						float lpow = pow(glm::length(pL.delta), 2);
						float mCutterStem = glm::dot(absCutterStem - absPartStem, pL.delta) / lpow;
						float mCutterEnd = glm::dot(absCutterEnd - absPartStem, pL.delta) / lpow;

						// Since this is in terms of pL.delta, a mag of 1 is = to part end and 0 = part stem
						sharedLine = rangeInRange<false, true>(0, 1, mCutterStem, mCutterEnd);
						
					}

					// With a cross of 0, the line test will never work. We'll skip it now
					pv = pv->edge->vert;
					continue;
				}

				testLineLine_t t = testLineLine(cL, pL, 0.001f);
				if (t.hit)
				{
					bool entering = glm::dot(glm::cross(pL.delta, cL.delta), partNorm) >= 0;
					if (entering)
					{
						// If we're entering, we want to ignore perfect intersections where the end of this cut line *just* hits a part line
						// It's 1 if it's a full filled delta
						if (closeTo(t.t1, 1))
						{
							// Drop it
							pv = pv->edge->vert;
							continue;
						}
					}
					else
					{
						// If we're exiting, we want to ignore perfect intersections where the stem of this cut line *just* hits a part line
						// It's 0 if it's a next to the stem
						if (closeTo(t.t1, 0))
						{
							// Drop it
							pv = pv->edge->vert;
							continue;
						}
					}
					// Store the intersection for later
					snipIntersect_t si
					{
						.entering = entering,
						.cutterT = t.t1,
						.intersect = t.intersect,
						.vert = pv,
						.edge = pv->edge,
					};
					intersections.push_back(si);
				}

				pv = pv->edge->vert;
			} while (pv != pvStart);
		}

		// Now that we have all of our intersections, we need to sort them all.
		// Sort by closest to stem
		std::qsort(intersections.data(), intersections.size(), sizeof(snipIntersect_t), [](void const* a, void const* b) -> int {
			const snipIntersect_t* siA = static_cast<const snipIntersect_t*>(a);
			const snipIntersect_t* siB = static_cast<const snipIntersect_t*>(b);
			if (siA->cutterT < siB->cutterT)
				return -1;
			else if (siA->cutterT > siB->cutterT)
				return 1;
			else
				return 0;
			});

		// Did we intersect?
		if (intersections.size())
		{
			for (int i = 0; i < intersections.size(); i++)
			{
				snipIntersect_t& si = intersections[i];

				// I would rather do this as a preprocess, but we can't determine when sharedLine will be true...
				if (sharedLine)
				{
					// If we're sharing a line, we can't be cutting into endpoints!
					if (glm::distance(si.intersect, *si.vert->vert + meshOrigin) < 0.001
						|| glm::distance(si.intersect, *si.edge->vert->vert + meshOrigin) < 0.001)
					{
						invalidateSkipOver = si.entering;
						// We're cutting an endpoint! Skip it!
						continue;
					}
				}

				if (si.entering)
				{
					// Starts a cut
					cutting = true;

					// Split the intersected edge
					glm::vec3* point = new glm::vec3();
					*point = si.intersect - meshOrigin;
					mesh.cutVerts.push_back(point);
					part->sliced->cutVerts.push_back(point);
					drag = splitHalfEdgeAtPoint(si.edge, point);

					// If we're at the end of our intersections, then we can just drag this all the way out.
					// There's no end cap right now
					if (intersections.size() - 1 == i)
					{
						// Drag to the end of this edge
						glm::vec3* end = new glm::vec3();
						*end = *cv->edge->vert->vert + cuttingMeshOrigin - meshOrigin;
						mesh.cutVerts.push_back(end);
						part->sliced->cutVerts.push_back(end);
						drag = dragEdge(drag, end);
					}
				}
				else
				{
					// Caps off a cut
					if (cutting)
					{
						// Split at intersection
						glm::vec3* point = new glm::vec3();
						*point = si.intersect - meshOrigin;
						mesh.cutVerts.push_back(point);
						part->sliced->cutVerts.push_back(point);
						draggable_t target = splitHalfEdgeAtPoint(si.edge, point);

						// Drag the edge into the other side
						dragEdgeInto(drag, target);

						// We should be done cutting this part now.
						cutting = false;

						// Split our edit into two different faces.
						// The one we think will get culled will be new
						
						// Clear ourselves so we can make sure we just have the data we need
						inCutFace_t* owner = (inCutFace_t*)target.outof->face;
						owner->cullDepth = 0;
						owner->edges.clear();
						owner->verts.clear();

						inCutFace_t* other = new inCutFace_t;
						other->cullDepth = -1;
						other->parent = part;
						cutFaces.push_back(other);

						faceFromLoop(target.outof, owner);
						faceFromLoop(target.into, other);

						didCut |= true;
					}
					else
					{
						// If we entered this via a shared line, we don't want to shift our cvStart
						if (!invalidateSkipOver)
						{
							// We're exciting now?
							// When not cutting?
							// Seems like we need to loop more!
							// Try to roll out of the mesh so another point can cut into it.
							cv = cv->edge->vert;
							cvStart = cv;
							fullRollOver++;
						}
						invalidateSkipOver = false;
					}

				}

				// Loop the intersections again, incase we sliced through two parts with one edge
			}
		}
		else
		{
			// We didn't intersect, but we might have to drag a cut

			// Continues a cut
			if (cutting)
			{
				// Create a new point within our edit space
				glm::vec3 editPoint = *cv->edge->vert->vert + cuttingMeshOrigin - meshOrigin;
				glm::vec3* newEditPoint = new glm::vec3();
				*newEditPoint = editPoint;
				mesh.cutVerts.push_back(newEditPoint);
				part->sliced->cutVerts.push_back(newEditPoint);

				// Drag the previous cut to our new location
				drag = dragEdge(drag, newEditPoint);
			}
		}

		cv = cv->edge->vert;
	} while (cv != cvStart && fullRollOver <= cutter->verts.size());
	// Loop back to the top and try on the newly added faces

	// This should not happen!
	SASSERT_S(fullRollOver <= cutter->verts.size());
	SASSERT_S(!cutting);

	// If this is one, we failed to cut anything! 
	if (cutFaces.size() == 1)
	{
	}
	else
	{
		for (int i = 0; i < cutFaces.size(); i++)
		{
			int j = 0;
			vertex_t* sv = cutFaces[i]->verts.front(), *v = sv;
			do
			{
				j++;
				v = v->edge->vert;
			} while (v != sv);
			SASSERT(j == cutFaces[i]->verts.size());
			SASSERT(j == cutFaces[i]->verts.size());

		}

		std::vector<face_t*> cleanFaces;
		// Cull off faces with < 0 depth
		for (int i = 0; i < cutFaces.size(); i++)
		{
			inCutFace_t* f = static_cast<inCutFace_t*>(cutFaces[i]);
			if (f->cullDepth < 0)
			{
				// Unlink twins
				for (auto e : f->edges)
					if (e->pair)
					{
						e->pair->pair = nullptr;
					}

				delete f;
			}
			else
				cleanFaces.push_back(f);
		}
		cutFaces = cleanFaces;
	}

	// Scrub off our sliced flags
	for(auto f : cutFaces)
		for(auto e : f->edges)
			e->flags &= ~EdgeFlags::EF_SLICED;

	return didCut;
}

// All faces should belong to one part!
pointInConvexTest_t faceInFacesQueryTest(face_t* inside, std::vector<face_t*>& faces)
{
	if (faces.size() == 0)
		return {};

	glm::vec3 shift = parentMesh(inside)->origin - parentMesh(faces.front())->origin;
	pointInConvexTest_t test;
	for (auto v : inside->verts)
	{
		pointInConvexTest_t t;
		for (auto c : faces)
		{
			t = pointInConvexLoopQueryIgnoreNonOuterEdges(c->verts.front(), *v->vert + shift);
			if (t.outside == 0)
				break;
		}
		if (t.outside)
			test.outside++;
		else if (t.onEdge)
			test.onEdge++;
		else
			test.inside++;
	}

	return test;
}

//#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#define DEBUG_PRINT(...) 

void applyCuts(cuttableMesh_t* mesh, std::vector<mesh_t*>& cutters)
{
	DEBUG_PRINT("\nSlicing!\n");
	// Clear out our old cut verts
	for (auto v : mesh->cutVerts)
		delete v;
	mesh->cutVerts.clear();

	// Clear out our old faces
	for (auto p : mesh->parts)
	{
		if (p->sliced)
		{
			delete p->sliced;
			p->sliced = nullptr;
		}
	}

	// No cutters, no cuts!
	if (cutters.size() == 0)
		return;

	// Iterate over faces and check if we have ones with opposing norms
	// We go part -> cutter -> slicer, so we can determine exactly what's going to cut this face up
	for (auto part : mesh->parts)
	{
		glm::vec3 partNorm = part->normal;
		std::vector<meshPart_t*> slicers;
		
		// Find candidates
		for (auto cutter : cutters)
		{
			for (auto slicer : cutter->parts)
			{
				glm::vec3 slicerNorm = slicer->normal;
				// Make it relative to what we're cutting
				glm::vec3 slicerPoint = *slicer->verts.front()->vert + cutter->origin - mesh->origin;


				glm::vec3 centerDiff = *part->verts.front()->vert - slicerPoint;

				// Flatten the diff to the axis of the normal, this way we can see how far the parts are from eachother.
				centerDiff *= partNorm;

				// Do we share an axis?
				if (glm::length(centerDiff) >= 0.01f)
					continue;

				// Do our normals actually oppose?
				if (!closeTo(glm::dot(slicerNorm, partNorm), -1))
					continue;
				
				// Good enough of a candidate!
				slicers.push_back(slicer);
			} 
		}


		// We need to perform collision tests so that we can determine which strategy of slicing we want.
		// We do this because all face snips must occur before all face cracks


		if (slicers.size() == 0)
			continue;

		// Make a vector for all of the new faces we'll be making, and clone into it something to work with
		std::vector<face_t*> cutFaces;
		inCutFace_t* copyCat = new inCutFace_t;
		cloneFaceInto(part, copyCat);
		copyCat->flags &= ~FaceFlags::FF_MESH_PART;
		copyCat->parent = part;
		cutFaces.push_back(copyCat);

		if (!part->sliced)
			part->sliced = new slicedMeshPartData_t;

		// Since cutting faces sometimes incurs a subdivision, we need to work on all faces
		for (int k = 0; k < cutFaces.size(); k++)
		{
			DEBUG_PRINT("- face %d/%d\n", k, cutFaces.size());

			face_t* face = cutFaces[k];
			std::vector<meshPart_t*> faceSlicers = slicers;
			
			bool didSlice = true;
			std::vector<face_t*> coll;
			do
			{
				std::vector<meshPart_t*> snipLater;

				for (int l = 0; l < faceSlicers.size(); l++)
				{

					// Really not fond of this, but we have to turn the currect face into a valid covex face for testing for each slice...
					// Any way to reuse this data?
					if (didSlice)
					{
						DEBUG_PRINT("-- Coll!\n");

						for (auto f : coll)
							delete f;
						coll.clear();
						copyCat = new inCutFace_t;
						cloneFaceInto(face, copyCat);
						coll.push_back(copyCat);
						convexifyMeshPartFaces(*part, coll);// , []() { return (face_t*)new inCutFace_t; });

						// Only want to regen this data when we need to
						didSlice = false;
					}

					meshPart_t* slicer = faceSlicers[l];
					mesh_t* slicerMesh = slicer->mesh;

					// Perform a collision test to see what algo we should use
					// We test all points of the slicer on our face. If any intersect, we need to snip.
					// If all are out, and any one point is in the slicer, we're engulfed.
					pointInConvexTest_t slicerInFace = faceInFacesQueryTest(slicer, coll);
					pointInConvexTest_t faceInSlicer = faceInFacesQueryTest(face, slicer->collision);

					//if ((test.onEdge == slicer->verts.size() || test.outside == slicer->verts.size()) && test.inside == 0)
					//{
						// We have *A* vert outside, we're not certain if we're totally out or not.
						// If any one vert out of the face at this, we're not engulfed.
						/*
						int pointsOutSlicer = 0;
						for (auto v : face->verts)
							if (!pointInConvexMeshPart(slicer, *v->vert - mesh->origin + slicerMesh->origin))
							{
								pointsOutSlicer++;
							}
						*/

					if (faceInSlicer.outside == 0 && (faceInSlicer.onEdge + faceInSlicer.inside == face->verts.size() ))
					{
						DEBUG_PRINT("-- Engulfed\n");
						// Engulfed faces need to get culled off and completely dropped
						cutFaces.erase(cutFaces.begin() + k);
						delete face;
						k--;
						goto fullBreak;
					}
					else if (faceInSlicer.outside == face->verts.size() && slicerInFace.outside == slicer->verts.size())
					{
						DEBUG_PRINT("-- Dropped\n");
						// Else, we're totally out of the slicer. Move along and don't remember this slicer
						continue;
					}
					else if ((slicerInFace.inside == slicer->verts.size() ) && slicerInFace.outside == 0)
					{
						// This face needs cracking, but it might be doable as a snip later... Store it for later
						snipLater.push_back(slicer);
						DEBUG_PRINT("-- Snip Later!\n");
						continue;
					}

					// We only want to work on this face!
					std::vector<face_t*> curFaceVec;
					curFaceVec.push_back(face);
					didSlice = faceSnips(*mesh, *slicerMesh, part, slicer, curFaceVec);
					DEBUG_PRINT("-- Snip!\n");

					// Record our new faces
					for (auto cf : curFaceVec)
						if (cf != face)
							cutFaces.push_back(cf);
				}

				if (snipLater.size() == faceSlicers.size())
				{
					// All laters! Let's do a face crack and try again.
					opposingFaceCrack(*mesh, part, faceSlicers.back(), face);
					didSlice = true;
					faceSlicers.pop_back();
					DEBUG_PRINT("-- Crack!\n");

				}
				else
				{
					// Diff in size! Let's slice again and see if we can get that number down again...
					faceSlicers = snipLater;
				}

			} while (faceSlicers.size());
		fullBreak:
			for (auto f : coll)
				delete f;
			coll.clear();
			DEBUG_PRINT("- Clear!\n");

		}
		

		// Mark em all as cut
		for (auto f : cutFaces)
			f->flags |= FaceFlags::FF_CUT;
		
		
		// Save our new faces

		part->sliced->faces.clear();
		for (auto f : cutFaces)
		{
			part->sliced->faces.push_back(f);
		}
		DEBUG_PRINT("-- Done!\n");

	}
}
#undef DEBUG_PRINT



void clearSlicedData(slicedMeshPartData_t* sliced)
{
	if (!sliced)
		return;
	for (auto f : sliced->collision)
		delete f;
	//for (auto f : sliced->tris)
	//	delete f;
	sliced->collision.clear();
	//sliced->tris.clear();
	sliced->cutVerts.clear(); // This makes referecens back into the mesh! Do not delete!
}

void fillSlicedData(meshPart_t* part)
{
	if (!part->sliced)
		part->sliced = new slicedMeshPartData_t;
	// Give ourselves a face to crack
	inCutFace_t* clone = new inCutFace_t;
	cloneFaceInto(part, clone);
	part->sliced->collision.push_back(clone);
}