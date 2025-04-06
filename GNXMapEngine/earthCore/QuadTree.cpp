#include "QuadTree.h"
#include "rendersystem/AABB.h"
#include "BoundingRegion.h"
#include "EarthNode.h"

EARTH_CORE_NAMESPACE_BEGIN

QuadNode::QuadNode(EarthNode* earthNode, QuadNode* parent, const Vector2d& vStart, const Vector2d& vEnd, uint32_t level, ChildRegion region)
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

	/*std::vector<Vector3d> points;
	points.reserve(2);
	points.push_back(point1);
	points.push_back(point2);

	mBoundingBox = AxisAlignedBoxd::FromPositions(points);*/

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
	
	// 判断瓦片和视锥体是否相交，相交的话去掉被裁剪的标记，否则加上被裁剪的标记

	Frustumd frustum;

// 	if (cubeInFrustum(
// 		mBoundingBox._minimum.x
// 		, mBoundingBox._maximum.x
// 		, mBoundingBox._minimum.y
// 		, mBoundingBox._maximum.y
// 		, mBoundingBox._minimum.z
// 		, mBoundingBox._maximum.z))
// 	{
// 		mStatusFlag &= ~FLAG_HAS_CULL;
// 	}
// 	else
// 	{
// 		mStatusFlag |= FLAG_HAS_CULL;
// 	}

	// 相机位置
	Vector3f eyePosition = camera->GetPosition();

	// 瓦片中心点
	Vector3d vWCenter = mBoundingBox.center;
	Vector3d min = mBoundingBox.minimum;
	Vector3d max = mBoundingBox.maximum;

	Vector3d vWSize = max - min;

	double fSize = vWSize.Length() * 0.5;
	double distance = (vWCenter - Vector3d(eyePosition.x, eyePosition.y, eyePosition.z)).Length();

	if (distance / fSize < 3 && HasNoFlag(mStatusFlag, FLAG_HAS_CULL))
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
	else if (distance / fSize > 3.45)
	{
		for (int i = 0; i < 4; ++i)
		{
			mChildNodes[i] = nullptr;
		}
	}
}

EARTH_CORE_NAMESPACE_END
