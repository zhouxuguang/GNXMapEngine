//
//  EarthNode.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#include "EarthNode.h"

#include <algorithm>
#include <cmath>

EARTH_CORE_NAMESPACE_BEGIN

EarthNode::EarthNode(const Ellipsoid& ellipsoid, EarthCameraPtr cameraPtr) : mEllipsoid(ellipsoid), mHorizonCulling(ellipsoid), mTileLoadPool(4)
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

	KeepCameraAboveTerrain();
	if (mCameraPtr)
		mHorizonCulling.SetCameraPosition(mCameraPtr->GetEyeCartesian());
	const HorizonCulling* culling = mCameraPtr && mHorizonCullingEnabled ? &mHorizonCulling : nullptr;
	for (size_t i = 0; i < mQuadNodes.size(); i ++)
	{
		mQuadNodes[i]->Update(mCameraPtr, culling);
	}

	if (KeepCameraAboveTerrain())
	{
		mHorizonCulling.SetCameraPosition(mCameraPtr->GetEyeCartesian());
		for (const auto& root : mQuadNodes)
		{
			root->Update(mCameraPtr, culling);
		}
	}
}

bool EarthNode::SampleTerrainHeight(double longitude, double latitude, double& height) const
{
	for (const auto& root : mQuadNodes)
	{
		if (root->SampleTerrainHeight(longitude, latitude, height))
		{
			return true;
		}
	}
	return false;
}

bool EarthNode::KeepCameraAboveTerrain()
{
	if (!mCameraPtr)
	{
		return false;
	}

	constexpr double clearance = 20.0;
	bool moved = false;
	for (int attempt = 0; attempt < 32; ++attempt)
	{
		const Geodetic3D& eye = mCameraPtr->GetEyeGeodetic();
		double terrainHeight = 0.0;
		if (!SampleTerrainHeight(eye.longitude, eye.latitude, terrainHeight) ||
			!std::isfinite(terrainHeight) || eye.height + 0.001 >= terrainHeight + clearance)
		{
			break;
		}

		const double distance = mCameraPtr->GetEyeDistance();
		const double deficit = terrainHeight + clearance - eye.height;
		const double vertical = std::max(0.05, std::cos(mCameraPtr->GetPitchAngleAtTarget()));
		const double step = std::max(deficit / vertical, distance * 0.05);
		const double nextDistance = std::min(distance + step, 20000000.0);
		if (nextDistance <= distance)
		{
			break;
		}
		mCameraPtr->SetEyeDistance(nextDistance);
		moved = true;
	}
	return moved;
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
