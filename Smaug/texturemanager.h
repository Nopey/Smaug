#pragma once

#include <bgfx/bgfx.h>
#include <map>
#include <string>
#include <memory>

class CTextureManager
{
public:
	CTextureManager();
	~CTextureManager();
	bgfx::TextureHandle LoadTexture(const char* path);
	bgfx::TextureHandle ErrorTexture();
private:
	// We map paths to textures as we load them so that we don't reload them later
	std::map<std::string, bgfx::TextureHandle> m_textureMap;
	std::unique_ptr<class CErrorTexture> m_errorTexture;
};

CTextureManager& TextureManager();
