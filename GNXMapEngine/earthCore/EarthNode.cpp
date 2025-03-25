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
	mCameraPtr = cameraPtr;

	// 开启异步加载数据的线程池
	mTileLoadPool.Start();
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

void EarthNode::Initialize()
{
	if (mInited)
	{
		return;
	}
	auto leftRoot = std::make_shared<earthcore::QuadNode>(this,
		nullptr
		, Vector2d(-M_PI, -M_PI_2)
		, Vector2d(0, M_PI_2)
		, 0
		, earthcore::QuadNode::CHILD_LT
	);
	auto rightRoot = std::make_shared<earthcore::QuadNode>(this,
		nullptr
		, Vector2d(0, -M_PI_2)
		, Vector2d(M_PI, M_PI_2)
		, 0
		, earthcore::QuadNode::CHILD_LT
	);

	mQuadNodes.push_back(leftRoot);
	mQuadNodes.push_back(rightRoot);
	mInited = true;
}

void EarthNode::RequestTile(QuadNode* node)
{
	for (auto& layer : mLayers)
	{
		auto task = layer->CreateTask(node->mTileID);
		if (!task)
		{
			continue;
		}
		mTileLoadPool.Execute(task);
	}
}

void EarthNode::CancelRequest(QuadNode* node)
{
	for (auto& layer : mLayers)
	{
		layer->DestroyTask(node->mTileID);
	}
}

EARTH_CORE_NAMESPACE_END
