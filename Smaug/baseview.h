#pragma once
#include "ibaseview.h"
#include "viewlist.h"

class CBaseView : public IBaseView
{
public:
	CBaseView(bgfx::ViewId viewId, int width, int height, uint32_t clearColor);
	~CBaseView();

	virtual void Draw(float dt);
	virtual void Update(float dt, float mx, float my) {}

	virtual ImTextureID GetImTextureID();

	bgfx::ViewId m_viewId;
	
	union
	{
		struct
		{
			bgfx::TextureHandle m_fbColorTexture;
			bgfx::TextureHandle m_fbDepthTexture;
		};
		bgfx::TextureHandle m_fbTextures[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
	};

	bgfx::FrameBufferHandle m_framebuffer = BGFX_INVALID_HANDLE;

	int m_width;
	int m_height;
	uint32_t m_clearColor;

	// Width/Height
	float m_aspectRatio;

	bool m_focused;
};
