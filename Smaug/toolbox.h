#pragma once

#include "basetool.h"
#include <vector>
#include <memory>

class CToolBox
{
public:
	CToolBox();

	void ShowToolBox();
	void Update(float dt, glm::vec3 mousePos);

	void SetTool(CBaseTool* tool);
	void SwitchToLast();

	// This should always be used instead of directly adding a tool to m_tools. It insures the tool is properly initialized and etc
	template<typename T>
	void RegisterTool();

	std::vector<std::unique_ptr<CBaseTool>> m_tools;
	CBaseTool* m_currentTool;
	CBaseTool* m_lastTool;

	// This is for quick hold key binded tools
	bool m_holdingTool;
	CBaseTool* m_pocketedTool;
};

template<typename T>
void CToolBox::RegisterTool()
{
	m_tools.push_back(std::make_unique<T>());
}
