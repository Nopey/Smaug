#include "worldeditor.h"
#include "raytest.h"
#include "tessellate.h"
#include "slice.h"
#include "mesh.h"
#include "log.h"

#include <glm/geometric.hpp>
#include <glm/common.hpp>

/////////////////////
// Safe References //
/////////////////////

// Node Reference
CNodeRef::CNodeRef()              : m_targetId(INVALID_NODE_ID) {}
CNodeRef::CNodeRef(nodeId_t id)   : m_targetId(id)              {}
CNodeRef::CNodeRef(CNode* node) { if (node) m_targetId = node->NodeID(); else m_targetId = INVALID_NODE_ID; }
bool CNodeRef::IsValid() const { return Node() != nullptr; }
CNode* CNodeRef::operator->() const { return GetWorldEditor().GetNode(m_targetId); }
CNode* CNodeRef::Node() const { return GetWorldEditor().GetNode(m_targetId); }
void CNodeRef::operator=(const CNodeRef& ref) { m_targetId = ref.m_targetId;}


CNodeMeshPartRef::CNodeMeshPartRef() : m_partId(INVALID_MESH_ID), m_node(INVALID_NODE_ID) {}
CNodeMeshPartRef::CNodeMeshPartRef(meshPart_t &part, CNodeRef node) : CNodeMeshPartRef()
{
	if (!node.IsValid())
		return;

	std::vector<clasp<meshPart_t>>& parts = node->m_mesh->parts;
	auto f = std::find(parts.begin(), parts.end(), part);
	if (f != parts.end())
	{
		// Found it!
		m_node = node;
		size_t id = f - parts.begin();
		SASSERT(id < MAX_MESH_ID);
		m_partId = id;
	}
}

const bool CNodeMeshPartRef::IsValid()
{
	return m_partId != MAX_MESH_ID && m_node.IsValid();
}

meshPart_t* CNodeMeshPartRef::operator->() const
{
	if(m_node->m_mesh->parts.size() > m_partId)
		return m_node->m_mesh->parts[m_partId].get();
	return nullptr;
}

CNodeMeshPartRef::operator meshPart_t* () const
{
	if (m_node->m_mesh->parts.size() > m_partId)
		return m_node->m_mesh->parts[m_partId].get();
	return nullptr;
}

CNodeMeshPartRef::operator meshPart_t const & () const
{
	return **this;
}


void CNodeMeshPartRef::operator=(const CNodeMeshPartRef& ref)
{
	m_node = ref.m_node;
	m_partId = ref.m_partId;
}


// Half Edge Reference
CNodeHalfEdgeRef::CNodeHalfEdgeRef() : m_heId(MAX_MESH_ID), m_part() {}

CNodeHalfEdgeRef::CNodeHalfEdgeRef(halfEdge_t &he, CNodeRef node) : CNodeHalfEdgeRef()
{
	if (!node.IsValid())
		return;
	m_part = { he.face.cast<meshPart_t>(), node };

	auto& edges = m_part->edges;
	auto f = std::find(edges.begin(), edges.end(), he);
	if (f != edges.end())
	{
		// Found it!
		size_t id = f - edges.begin();
		SASSERT(id < MAX_MESH_ID);
		m_heId = id;
	}
}

const bool CNodeHalfEdgeRef::IsValid()
{
	return m_part.IsValid() && m_part->edges.size() > m_heId && m_part->edges[m_heId];
}

halfEdge_t* CNodeHalfEdgeRef::operator->() const
{
	if(m_part && m_part->edges.size() > m_heId)
		return m_part->edges[m_heId].get();
	return nullptr;
}

CNodeHalfEdgeRef::operator halfEdge_t* () const
{
	if (m_part && m_part->edges.size() > m_heId)
		return m_part->edges[m_heId].get();
	return nullptr;
}

void CNodeHalfEdgeRef::operator=(const CNodeHalfEdgeRef& ref)
{
	m_heId = ref.m_heId;
	m_part = ref.m_part;
}


CNodeVertexRef::CNodeVertexRef() : m_vertId(MAX_MESH_ID), m_part() {}
CNodeVertexRef::CNodeVertexRef(vertex_t &vertex, CNodeRef node) : CNodeVertexRef()
{
	if (!node.IsValid())
		return;
	m_part = { vertex.edge->face.cast<meshPart_t>(), node };

	std::vector<clasp<vertex_t>>& verts = m_part->verts;
	auto f = std::find(verts.begin(), verts.end(), vertex);
	if (f != verts.end())
	{
		// Found it!
		size_t id = f - verts.begin();
		SASSERT(id < MAX_MESH_ID);
		m_vertId = id;
	}
}

bool CNodeVertexRef::IsValid()
{
	return m_part.IsValid() && m_part->verts.size() > m_vertId && m_part->verts[m_vertId];
}

vertex_t* CNodeVertexRef::operator->() const
{
	if (m_part && m_part->verts.size() > m_vertId)
		return m_part->verts[m_vertId].get();
	return nullptr;
}

CNodeVertexRef::operator vertex_t* () const
{
	if (m_part && m_part->verts.size() > m_vertId)
		return m_part->verts[m_vertId].get();
	return nullptr;
}

void CNodeVertexRef::operator=(const CNodeVertexRef& ref)
{
	m_vertId = ref.m_vertId;
	m_part = ref.m_part;
}

static CWorldEditor *s_pWorldEditor = nullptr;

CWorldEditor::CWorldEditor()
{
	SASSERT_FATAL(!s_pWorldEditor);
	s_pWorldEditor = this;
	m_currentNodeId = 0;
}

CWorldEditor::~CWorldEditor()
{
	s_pWorldEditor = nullptr;
}

CWorldEditor& GetWorldEditor()
{
	SASSERT_FATAL(s_pWorldEditor);
	return *s_pWorldEditor;
}


void CWorldEditor::Clear()
{
	m_currentNodeId = 0;
	m_nodes.clear();
}

void CWorldEditor::RegisterNode(std::unique_ptr<CNode> node)
{
	node->m_id = m_currentNodeId;
	m_nodes.emplace(m_currentNodeId, std::move(node));
	m_currentNodeId++;

	// If this ever happens, it'll be awful.
	SASSERT(m_currentNodeId != INVALID_NODE_ID);
}

bool CWorldEditor::AssignID(std::unique_ptr<CNode> node, nodeId_t id)
{
	// If we're one ahead of the cur, increment it. It'll make life easier
	if (id == m_currentNodeId + 1)
		m_currentNodeId++;
	
	// We're assigning an arbitrary id. Check if it's not in use.
	if (m_nodes.contains(id))
	{
		// Failure!
		return false;
	}

	// Is this free?
	nodeId_t oldId = node->m_id;
	if (oldId != INVALID_NODE_ID)
	{
		if (m_nodes.contains(oldId))
		{
			// Remove the existing ref
			m_nodes.erase(oldId);

		}
	}

	node->m_id = id;
	m_nodes.emplace(id, std::move(node));

	return true;
}

void CWorldEditor::DeleteNode(CNode* node)
{
	if(!node) return;

	nodeId_t id = node->m_id;
	if (id != INVALID_NODE_ID)
	{
		if (m_nodes.contains(id))
		{
			m_nodes.erase(id);
			return;
		}
	}

	Log::Warn("DeleteNode called with unregistered node\n");
}

CQuadNode* CWorldEditor::CreateQuad()
{
	std::unique_ptr<CQuadNode> node = std::make_unique<CQuadNode>();
	CQuadNode *pNode = node.get();
	RegisterNode(std::move(node));
	return pNode;
}

CNode* CWorldEditor::GetNode(nodeId_t id)
{
	if(!m_nodes.contains(id))
		return nullptr;
	return m_nodes[id].get();
}

/*
CTriNode* CWorldEditor::CreateTri()
{
	std::unique_ptr<CTriNode> node = std::make_unique<CTriNode>();
	CTriNode *pNode = node.get();
	RegisterNode(std::move(node));
	return pNode;
}
*/


CNode::CNode(clasp<cuttableMesh_t>&& mesh) : m_mesh(std::move(mesh)), m_renderData(m_mesh), m_id(INVALID_NODE_ID), m_visible(true)
{
	/*
	m_sides = new nodeSide_t[m_sideCount];
	LinkSides();

	m_origin = glm::vec3(0, 0, 0);
	
	m_nodeHeight = 10;
	ConstructWalls();
	*/

	for(auto &p : m_mesh->parts)
		defineMeshPartFaces(p);
	CalculateAABB();
}

void CNode::PreviewUpdate()
{
	PreviewUpdateThisOnly();

	// Update what we're cutting
	for (auto &m : m_cutting)
	{
		m->PreviewUpdateThisOnly();
	}
}

void CNode::PreviewUpdateThisOnly()
{
	CalculateAABB();


	for (auto &pa : m_mesh->parts)
	{
		defineMeshPartFaces(pa);
		convexifyMeshPartFaces(pa, pa->collision);
		optimizeParallelEdges(pa, pa->collision);
	}

	std::vector<clasp_ref<mesh_t>> cutters;
	for (auto const &[_, node] : GetWorldEditor().m_nodes)
		if (node.get() != this)
			cutters.push_back(node->m_mesh.borrow());
	applyCuts(m_mesh, cutters);

	for (auto &pa : m_mesh->parts)
	{
		
		/*
		for (auto &cf : pa->collision)
			delete cf;
		pa->collision.clear();

		for (auto &cf : pa->sliced ? pa->sliced->collision : pa->)
		{
			face_t* f = new face_t;
			cloneFaceInto(cf, f);
			pa->collision.push_back(f);
		}
		convexifyMeshPartFaces(*pa, pa->collision);
		*/

		//for (auto m : m_mesh.parts)
		
		if (pa->sliced)
		{
			optimizeParallelEdges(pa, pa->sliced->faces);
			for (auto &cf : pa->sliced->faces)
			{
				std::vector<clasp<face_t>> temp;
				temp.push_back(make_clasp<face_t>());
				auto f = temp.back().borrow();
				cloneFaceInto(cf, f);
				f->parent = cf;
				convexifyMeshPartFaces(pa, temp);
				optimizeParallelEdges(pa, temp);
				for (auto& t : temp)
				{
					pa->sliced->collision.emplace_back();
					std::swap(pa->sliced->collision.back(), t);
				}
			}
		}

		pa->tris.clear();

		
		for (auto &cf : pa->sliced ? pa->sliced->collision : pa->collision)
		{
			pa->tris.push_back(make_clasp<face_t>());
			auto f = pa->tris.back().borrow();
			cloneFaceInto(cf, f);
			graphFace( f );
		}


		triangluateMeshPartConvexFaces(pa, pa->tris);
	}

	m_renderData.RebuildRenderData();
}

void CNode::Update()
{
	
	UpdateThisOnly();

	// Update what we're cutting
	for (auto &m : m_cutting)
	{
		m->UpdateThisOnly();
	}

}
void CNode::UpdateThisOnly()
{
	recenterMesh(m_mesh);
	PreviewUpdate();
}


bool CNode::IsPointInAABB(glm::vec3 point) const
{
	return testPointInAABB(point, GetAbsAABB(), 1.25f);
}

aabb_t CNode::GetAbsAABB() const
{
	aabb_t aabb = m_aabb;
	aabb.min += m_mesh->origin;
	aabb.max += m_mesh->origin;
	return aabb;
}


void CNode::ConnectTo(CNodeRef node)
{
	if (node.IsValid())
	{
		node->m_cutters.emplace(this);
		node->m_cutting.emplace(this);
	}
	m_cutters.emplace(node);
	m_cutting.emplace(node);
}

void CNode::DisconnectFrom(CNodeRef node)
{
	if (node.IsValid())
	{
		node->m_cutters.erase(this);
		node->m_cutting.erase(this);
	}
	m_cutters.erase(node);
	m_cutting.erase(node);
}

void CNode::CalculateAABB()
{
	m_aabb = meshAABB(m_mesh);

	//m_aabbLength = glm::length(m_aabb.max - m_aabb.min);

	//printf("AABB - Max:{%f, %f, %f} - Min:{%f, %f, %f}\n", m_aabb.max.x, m_aabb.max.y, m_aabb.max.z, m_aabb.min.x, m_aabb.min.y, m_aabb.min.z);
}

static clasp<cuttableMesh_t> make_quad_mesh() {
	clasp<cuttableMesh_t> mesh = make_clasp<cuttableMesh_t>();

	glm::vec3 points[] = {

	{-1,-1,-1}, // bottom back left
	{ 1,-1,-1}, // bottom back right
	{ 1, 1,-1}, // top back right
	{-1, 1,-1}, // top back left

	{-1,-1, 1}, // bottom front left
	{ 1,-1, 1}, // bottom front right
	{ 1, 1, 1}, // top front right
	{-1, 1, 1}, // top front left

	};

	auto p = addMeshVerts(mesh, points);

	clasp_ref<glm::vec3> front[] = { p[7], p[6], p[5], p[4] };
	addMeshFace(mesh.borrow(), front);

	clasp_ref<glm::vec3> back[] = { p[0], p[1], p[2], p[3] };
	addMeshFace(mesh.borrow(), back);

	clasp_ref<glm::vec3> left[] = { p[3], p[7], p[4], p[0] };
	addMeshFace(mesh.borrow(), left);

	clasp_ref<glm::vec3> right[] = { p[2], p[1], p[5], p[6] };
	addMeshFace(mesh.borrow(), right);

	clasp_ref<glm::vec3> bottom[] = { p[4], p[5], p[1], p[0] };
	addMeshFace(mesh.borrow(), bottom);

	clasp_ref<glm::vec3> top[] = { p[3], p[2], p[6], p[7] };
	addMeshFace(mesh.borrow(), top);

	return mesh;
}

CQuadNode::CQuadNode() : CNode(make_quad_mesh())
{
}
