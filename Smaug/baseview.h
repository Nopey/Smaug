#pragma once
#include "ibaseview.h"
#include "viewlist.h"

class CBaseView : public IBaseView
{
public:
	virtual void Init(bgfx::ViewId viewId, int width, int height, uint32_t clearColor) override;

	virtual void Draw(float dt) override;
	virtual void Update(float dt, float mx, float my) override {}

	virtual ImTextureID GetImTextureID() override;

	bgfx::ViewId m_viewId;
	
	union
	{
		struct
		{
			bgfx::TextureHandle m_fbColorTexture;
			bgfx::TextureHandle m_fbDepthTexture;
		};
		bgfx::TextureHandle m_fbTextures[2];
	};

	bgfx::FrameBufferHandle m_framebuffer;

	int m_width;
	int m_height;
	uint32_t m_clearColor;

	// Width/Height
	float m_aspectRatio;

	bool m_focused;
};
