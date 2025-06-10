#include "QuadTree.h"
#include "rendersystem/AABB.h"
#include "BoundingRegion.h"
#include "EarthNode.h"
#include "RenderSystem/RenderParameter.h"

EARTH_CORE_NAMESPACE_BEGIN

QuadNode::QuadNode(EarthNode* earthNode, QuadNode* parent, const Vector2d& vStart, const Vector2d& vEnd, uint32_t level, ChildRegion region) : mDemData(Ellipsoid::WGS84)
{
	mEarthNode = earthNode;
	mRegion = region;
	mParent = parent;
	mLLStart = vStart;
	mLLEnd = vEnd;

	Vector2d xLLCenter = GetLonLatCenter();
	// 计算瓦片ID，并赋值
	mTileID = GetTileID(level, xLLCenter.x, xLLCenter.y);

	// 计算瓦片的世界坐标范围
	Ellipsoid wgs84 = Ellipsoid::WGS84;

	Geodetic3D llPoint1 = Geodetic3D(mLLStart.x, mLLStart.y);
	Vector3d point1 = wgs84.CartographicToCartesian(llPoint1);

	Geodetic3D llPoint2 = Geodetic3D(mLLEnd.x, mLLEnd.y);
	Vector3d point2 = wgs84.CartographicToCartesian(llPoint2);

	GlobeRectangle globeRec(mLLStart.x, mLLStart.y, mLLEnd.x, mLLEnd.y);
	BoundingRegion geoBound(globeRec, 0, 0, wgs84);
	mBoundingBox = geoBound.getBoundingBox().ToAxisAligned();

	mDemData.SetStartEndGeoCoord(vStart, vEnd);
	mDemData.FillFace();
	mDemData.FillUV(Vector2f(0.0f, 1.0f), Vector2f(1.0f, 0.0f));

	// 计算瓦片局部偏移的矩阵的uniform
	mStartPoint = wgs84.CartographicToCartesian(Geodetic3D(mLLStart.x, mLLStart.y, 0));

	cbPerObject modelMatrix;
	modelMatrix.MATRIX_M = mathutil::Matrix4x4f::CreateTranslate(mStartPoint.x, mStartPoint.y, mStartPoint.z);
	modelMatrix.MATRIX_M_INV = modelMatrix.MATRIX_M.Inverse();
	mLocalUniform = getRenderDevice()->createUniformBufferWithSize(sizeof(cbPerObject));
	mLocalUniform->setData(&modelMatrix, 0, sizeof(cbPerObject));

	mChildNodes[0] = nullptr;
	mChildNodes[1] = nullptr;
	mChildNodes[2] = nullptr;
	mChildNodes[3] = nullptr;

	mEarthNode->RequestTile(this);
}

QuadNode::~QuadNode()
{
	mEarthNode->CancelRequest(this);

	for (int i = 0; i < 4; ++i)
	{
		mChildNodes[i] = nullptr;
	}
}

// 判断是否有子节点
inline bool QuadNode::HasChild() const
{
	return mChildNodes[0] != nullptr;
}

// 四叉树节点的经纬度中心点
inline Vector2d QuadNode::GetLonLatCenter() const
{
	return  (mLLStart + mLLEnd) * 0.5;
}

// 四叉树节点的经纬度范围
inline Vector2d QuadNode::GetLonLatRange() const
{
	return (mLLEnd - mLLStart);
}

void QuadNode::Update(const EarthCameraPtr& camera)
{
	if (!camera)
	{
		return;
	}

	if (mDemData.IsInited() && !mInited)
	{
		mVertexBuffer = getRenderDevice()->createVertexBufferWithBytes(mDemData.GetVertData(), mDemData.GetVertBytes(), RenderCore::StorageModePrivate);
		mIndexBuffer = getRenderDevice()->createIndexBufferWithBytes(mDemData.GetFaceData(), mDemData.GetFaceBytes(), RenderCore::IndexType_UShort);
		mInited = true;
	}
	
	// 判断瓦片和视锥体是否相交，相交的话去掉被裁剪的标记，否则加上被裁剪的标记
	Frustumd frustum;
	Matrix4x4d coloMatrix;
	Matrix4x4f viewProjMat = camera->GetProjectionMatrix() * camera->GetViewMatrix();
	for (uint16_t i = 0; i < 4; i ++)
	{
		for (uint16_t j = 0; j < 4; j++)
		{
			coloMatrix[i][j] = viewProjMat[i][j];
		}
	}
	frustum.initFrustum(coloMatrix);

 	/*if (!frustum.isOutOfFrustum(mBoundingBox))
 	{
 		mStatusFlag &= ~FLAG_HAS_CULL;
 	}
 	else
 	{
 		mStatusFlag |= FLAG_HAS_CULL;
 	}*/

	// 相机位置
	Vector3f eyePosition = camera->GetPosition();

	// 瓦片中心点
	Vector3d vWCenter = mBoundingBox.center;
	Vector3d min = mBoundingBox.minimum;
	Vector3d max = mBoundingBox.maximum;

	Vector3d vWSize = max - min;

	double fSize = vWSize.Length() * 0.5;
	double distance = (vWCenter - Vector3d(eyePosition.x, eyePosition.y, eyePosition.z)).Length();

	if (distance / fSize < 1 && HasNoFlag(mStatusFlag, FLAG_HAS_CULL))
	{
		if (!HasChild() && HasImage(mStatusFlag))
		{
			Vector2d vLlCenter = GetLonLatCenter();
			Vector2d vLLHalf = GetLonLatRange() * 0.5;

			// 开始分裂出新的瓦片

			mChildNodes[CHILD_LT] = std::make_shared<QuadNode>(mEarthNode, this
				, Vector2d(vLlCenter.x - vLLHalf.x, vLlCenter.y)
				, Vector2d(vLlCenter.x, vLlCenter.y + vLLHalf.y)
				, mTileID.level + 1
				, CHILD_LT
			);

			mChildNodes[CHILD_RT] = std::make_shared<QuadNode>(mEarthNode, this
				, Vector2d(vLlCenter.x, vLlCenter.y)
				, Vector2d(vLlCenter.x + vLLHalf.x, vLlCenter.y + vLLHalf.y)
				, mTileID.level + 1
				, CHILD_RT
			);

			mChildNodes[CHILD_LB] = std::make_shared<QuadNode>(mEarthNode, this
				, Vector2d(vLlCenter.x - vLLHalf.x, vLlCenter.y - vLLHalf.y)
				, Vector2d(vLlCenter.x, vLlCenter.y)
				, mTileID.level + 1
				, CHILD_LB
			);

			mChildNodes[CHILD_RB] = std::make_shared<QuadNode>(mEarthNode, this
				, Vector2d(vLlCenter.x, vLlCenter.y - vLLHalf.y)
				, Vector2d(vLlCenter.x + vLLHalf.x, vLlCenter.y)
				, mTileID.level + 1
				, CHILD_RB
			);
		}
		else
		{
			for (int i = 0; i < 4; ++i)
			{
				if (mChildNodes[i] && HasNoFlag(mStatusFlag, FLAG_HAS_CULL))
				{
					mChildNodes[i]->Update(camera);
				}
				else
				{
					mStatusFlag &= FLAG_RENDER;
				}
			}
		}
	}
	else if (distance / fSize > 1.45)
	{
		for (int i = 0; i < 4; ++i)
		{
			mChildNodes[i] = nullptr;
		}
	}
}

void QuadNode::GetRenderableNodes(QuadNodeArray& nodes)
{
	if (HasChild())
	{
		mChildNodes[0]->GetRenderableNodes(nodes);
		mChildNodes[1]->GetRenderableNodes(nodes);
		mChildNodes[2]->GetRenderableNodes(nodes);
		mChildNodes[3]->GetRenderableNodes(nodes);
	}
	else
	{
		if (HasFlag(mStatusFlag, FLAG_RENDER) && HasNoFlag(mStatusFlag, FLAG_HAS_CULL))
		{
			nodes.push_back(this);
		}
	}
}

EARTH_CORE_NAMESPACE_END
