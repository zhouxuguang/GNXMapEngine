//
//  EarthNode.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#include "EarthNode.h"

EARTH_CORE_NAMESPACE_BEGIN

EarthNode::EarthNode(const Ellipsoid& ellipsoid, EarthCameraPtr cameraPtr) : mEllipsoid(ellipsoid)
{
	auto leftRoot = std::make_shared<earthcore::QuadNode>(
		nullptr
		, Vector2d(-M_PI, -M_PI_2)
		, Vector2d(0, M_PI_2)
		, 0
		, earthcore::QuadNode::CHILD_LT
	);
	auto rightRoot = std::make_shared<earthcore::QuadNode>(
		nullptr
		, Vector2d(0, -M_PI_2)
		, Vector2d(M_PI, M_PI_2)
		, 0
		, earthcore::QuadNode::CHILD_LT
	);

	mQuadNodes.push_back(leftRoot);
	mQuadNodes.push_back(rightRoot);

	mCameraPtr = cameraPtr;
}

EarthNode::~EarthNode()
{
    //
}

void EarthNode::Update(float deltaTime)
{
    SceneNode::Update(deltaTime);

	for (size_t i = 0; i < mQuadNodes.size(); i ++)
	{
		mQuadNodes[i]->Update(mCameraPtr);
	}
}

EARTH_CORE_NAMESPACE_END
