#ifndef RENDER_SYSTEM_INCLUDE__H
#define RENDER_SYSTEM_INCLUDE__H

#include <array>
#include <vector>
#include "Engine/Header/GameManager.h"

class GraphicEngine;
class MeshRenderer;

class RenderSystem
{
public:
	RenderSystem(GraphicEngine* pGraphic);
	~RenderSystem() = default;
	void Rendering();

private:
	std::array<std::vector<MeshRenderer const*>, 16> m_meshRenderers{};
	GraphicEngine* m_pGraphic;

	friend class Scene;
	friend class GameManager;
};

#endif // !RENDER_SYSTEM_INCLUDE__H
