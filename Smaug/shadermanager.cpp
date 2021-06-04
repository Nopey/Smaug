#include "shadermanager.h"
#include "utils.h"
#include <glm/vec4.hpp>
#include <cstdio>

static struct
{
	const char* fragmentShader;
	const char* vertexShader;
	bgfx::ProgramHandle programHandle;
} 
s_programInfo[] = {
	{nullptr, nullptr}, // Shader::NONE

	{"fs_preview_view.bin", "vs_preview_view.bin"}, // Shader::WORLD_PREVIEW_SHADER
	{"fs_model.bin",        "vs_model.bin"}, // Shader::EDIT_VIEW_SHADER
	{"fs_model.bin",        "vs_model.bin"}, // Shader::CURSOR_SHADER
	{"fs_model.bin",        "vs_model.bin"}, // Shader::MODEL_SHADER
	{"fs_line.bin",         "vs_line.bin" }, // Shader::ERROR_MODEL_SHADER
	{"fs_grid.bin",         "vs_grid.bin" }, // Shader::GRID
	{"fs_line.bin",         "vs_line.bin" }, // Shader::LINE
	
};

CShaderManager *s_pShaderManager = nullptr;

CShaderManager::CShaderManager()
{
	SASSERT_FATAL(!s_pShaderManager);
	s_pShaderManager = this;

	for (int i = 1; i < (int)Shader::COUNT; i++)
	{
		
		s_programInfo[i].programHandle = LoadShader(s_programInfo[i].fragmentShader, s_programInfo[i].vertexShader);
		
		if(s_programInfo[i].programHandle.idx == bgfx::kInvalidHandle)
		{
			Log::Fault("Failed to load program: fragment: %-20s  vertex: %-20s\n", s_programInfo[i].fragmentShader,
				s_programInfo[i].vertexShader);
		}
		
	}

	m_colorUniform = bgfx::createUniform("color", bgfx::UniformType::Vec4);
}

CShaderManager::~CShaderManager()
{
	for (int i = 1; i < (int)Shader::COUNT; i++)
	{
		if (s_programInfo[i].programHandle.idx != bgfx::kInvalidHandle)
		{
			bgfx::destroy(s_programInfo[i].programHandle);
			s_programInfo[i].programHandle = BGFX_INVALID_HANDLE;
		}
	}

	bgfx::destroy(m_colorUniform);
	m_colorUniform = BGFX_INVALID_HANDLE;

	s_pShaderManager = nullptr;
}

CShaderManager& ShaderManager()
{
	SASSERT_FATAL(s_pShaderManager);
	return *s_pShaderManager;
}


void CShaderManager::SetColor(glm::vec4 color)
{
	bgfx::setUniform(m_colorUniform, &color);
}

bgfx::ProgramHandle CShaderManager::GetShaderProgram(Shader shader)
{
	// Out of range. Return invalid handle
	if (shader <= Shader::NONE || shader > Shader::COUNT)
	{
		return BGFX_INVALID_HANDLE;
	}

	return s_programInfo[(int)shader].programHandle;
}
