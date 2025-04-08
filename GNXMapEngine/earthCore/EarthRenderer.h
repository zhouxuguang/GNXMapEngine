//
//  EarthRenderer.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_MAPENGINE_EARTHRENDERER_INCLUDE_GJFJGJDFNDF
#define GNX_MAPENGINE_EARTHRENDERER_INCLUDE_GJFJGJDFNDF

#include "QuadTree.h"
#include "RenderSystem/mesh/MeshRenderer.h"

EARTH_CORE_NAMESPACE_BEGIN

class EarthRenderer : public MeshRenderer
{
public:
	EarthRenderer();

	~EarthRenderer();

	void SetRendererNodes(const QuadNode::QuadNodeArray& nodes);

	virtual void Render(RenderInfo& renderInfo);

private:
	//QuadNode::QuadNodeArray mNodes;

	TextureSamplerPtr mSampler = nullptr;
};

EARTH_CORE_NAMESPACE_END

#endif