//
//  EarthRenderer.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_MAPENGINE_EARTHRENDERER_INCLUDE_GJFJGJDFNDF
#define GNX_MAPENGINE_EARTHRENDERER_INCLUDE_GJFJGJDFNDF

#include "QuadTree.h"
#include "Runtime/RenderSystem/include/mesh/MeshRenderer.h"
#include "Runtime/RenderSystem/include/DeferredGeometry.h"

EARTH_CORE_NAMESPACE_BEGIN

class EarthRenderer : public MeshRenderer, public RenderSystem::DeferredGeometryProvider
{
public:
	EarthRenderer();

	~EarthRenderer();

	void SetRendererNodes(const QuadNode::QuadNodeArray& nodes);

	void Render(RenderInfo& renderInfo) override;
	void CollectDeferredGeometry(std::vector<RenderSystem::DeferredGeometryDraw>& draws) override;

	void SetLightingEnabled(bool enabled) { mLightingEnabled = enabled; }
	bool IsLightingEnabled() const { return mLightingEnabled; }

private:
	//QuadNode::QuadNodeArray mNodes;

	TextureSamplerPtr mSampler = nullptr;
	bool mLightingEnabled = false;
};

EARTH_CORE_NAMESPACE_END

namespace RenderSystem
{
template<> struct ComponentTypeOf<earthcore::EarthRenderer>
{
    static constexpr ComponentType Value = ComponentType::MeshRenderer;
};
}

#endif
