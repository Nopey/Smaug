#include "meshrenderer.h"
#include "utils.h"

static bgfx::VertexLayout MeshVertexLayout()
{
	static bgfx::VertexLayout layout;
	static bool init = true;
	if (init)
	{
		init = false;
		layout.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.end();
	}
	
	return layout;
}


CMeshRenderer::CMeshRenderer(mesh_t& mesh) : m_mesh(mesh)
{
}

CMeshRenderer::~CMeshRenderer()
{
	bgfx::destroy(m_vertexBuf);
	bgfx::destroy(m_indexBuf);
}

void CMeshRenderer::RebuildRenderData()
{
	const bgfx::Memory* vertBuf;
	const bgfx::Memory* indexBuf;

	BuildRenderData(vertBuf, indexBuf);

	if (m_vertexBuf.idx != bgfx::kInvalidHandle)
	{
		bgfx::update(m_vertexBuf, 0, vertBuf);
		bgfx::update(m_indexBuf, 0, indexBuf);
	}
	else
	{
		m_vertexBuf = bgfx::createDynamicVertexBuffer(vertBuf, MeshVertexLayout(), BGFX_BUFFER_ALLOW_RESIZE);
		m_indexBuf = bgfx::createDynamicIndexBuffer(indexBuf, BGFX_BUFFER_ALLOW_RESIZE);
	}
}

void CMeshRenderer::Render()
{
	bgfx::setVertexBuffer(0, m_vertexBuf);
	bgfx::setIndexBuffer(m_indexBuf, 0, m_indexCount);
	// This does not bgfx::submit!!
}

void CMeshRenderer::Render(CModelTransform& trnsfm)
{
	glm::mat4 mtx = trnsfm.Matrix();

	bgfx::setTransform(&mtx[0][0]);
	Render();
	// This does not bgfx::submit!!
}


void CMeshRenderer::BuildRenderData(const bgfx::Memory*& vertBuf, const bgfx::Memory*& indexBuf)
{
	// We can only render tris!
	if (m_mesh.verts.size() < 3)
		return;

	int indexCount = 0;
	for (auto p : m_mesh.parts)
	{
		int vCount = p->verts.size();

		triangluateMeshPartFaces(*p);
		indexCount += p->faces.size() * 3; // 3 indexes per tri
	}

	// Allocate our buffers
	// bgfx cleans these up for us
	vertBuf = bgfx::alloc(m_mesh.verts.size() * sizeof(glm::vec3));
	indexBuf = bgfx::alloc(indexCount * sizeof(uint16_t)); // Each face is a tri

	uint16_t* idxData = (uint16_t*)indexBuf->data;

	int vOffset = 0;
	for (glm::vec3* v : m_mesh.verts)
	{
		memcpy(vertBuf->data + vOffset, v, sizeof(glm::vec3));
		vOffset += sizeof(glm::vec3);
	}

	int iOffset = 0;

	for (auto p : m_mesh.parts)
		for (auto f : p->faces)
		{
			/*
			// If our verts are out of order, this will fail. Should we use the HE data to determine this?
			halfEdge_t* he = f->edges[0];
			do
			{

				uint16_t vert = he->vert->vert - vertData;
				if (vert > m_part.verts.size())
					printf("[MeshRenderer] Mesh refering to vert not in list!!\n");

				idxData[offset] = vert;
				offset++;
				he = he->next;
			} while (he != f->edges[0]);
			*/
			for (auto v : f->verts)
			{
				uint16_t vert = 0;// v->vert - vertData;
				if (vert > m_mesh.verts.size())
					printf("[MeshRenderer] Mesh refering to vert not in list!!\n");

				idxData[iOffset] = vert;
				iOffset++;
			}

		}
	m_indexCount = iOffset;

}

}