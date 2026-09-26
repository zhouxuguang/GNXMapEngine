//
//  EarthNode.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#include "EarthNode.h"

#include <algorithm>

EARTH_CORE_NAMESPACE_BEGIN

EarthNode::EarthNode(const Ellipsoid& ellipsoid, EarthCameraPtr cameraPtr) : mEllipsoid(ellipsoid), mTileLoadPool(4)
{
	mCameraPtr = cameraPtr;

	// 开启异步加载数据的线程池
	mTileLoadPool.Start();
}

EarthNode::~EarthNode()
{
    // 四叉树节点析构时会回调 CancelRequest -> mLayers，必须保证 mLayers 还活着。
    // 成员析构顺序与声明顺序相反（mLayers 先于 mQuadNodes 被销毁），所以这里
    // 主动在析构体里先释放四叉树。
    mQuadNodes.clear();
}

void EarthNode::Update(float deltaTime)
{
    SceneNode::Update(deltaTime);

    mPendingTextureUploads.erase(
        std::remove_if(mPendingTextureUploads.begin(), mPendingTextureUploads.end(),
            [](const RenderCore::TextureUploadPtr& upload) {
                return !upload || upload->GetStatus() != RenderCore::TextureUploadStatus::Pending;
            }),
        mPendingTextureUploads.end());

	for (size_t i = 0; i < mQuadNodes.size(); i ++)
	{
		mQuadNodes[i]->Update(mCameraPtr);
	}
}

void EarthNode::TrackTextureUpload(const RenderCore::TextureUploadPtr& upload)
{
    if (upload && upload->GetStatus() == RenderCore::TextureUploadStatus::Pending)
        mPendingTextureUploads.push_back(upload);
}

void EarthNode::GetAllRendererNodes(QuadNode::QuadNodeArray& quadNodes)
{
	for (size_t i = 0; i < mQuadNodes.size(); i++)
	{
		mQuadNodes[i]->GetRenderableNodes(quadNodes);
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

uint32_t EarthNode::RequestTile(QuadNode* node)
{
	if (!node)
	{
		return 0;
	}

	uint32_t createdTaskCount = 0;
	for (auto& layer : mLayers)
	{
		auto task = layer->CreateTask(node, node->mLoadState);
		if (!task)
		{
			continue;
		}
		mTileLoadPool.Execute(task);
		++createdTaskCount;
	}

	GetQuadTreeStats().requests += createdTaskCount;
	return createdTaskCount;
}

void EarthNode::CancelRequest(QuadNode* node)
{
	for (auto& layer : mLayers)
	{
		layer->DestroyTask(node->mTileID);
	}
}

EARTH_CORE_NAMESPACE_END
