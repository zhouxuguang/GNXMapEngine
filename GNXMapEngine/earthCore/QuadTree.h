#ifndef GNX_MAP_ENGINE_QUADTREE_INCLUDE_GJGJDF
#define GNX_MAP_ENGINE_QUADTREE_INCLUDE_GJGJDF

#include "EarthEngineDefine.h"
#include "Vector3.h"
#include "EarthCamera.h"
#include "QuadTileID.h"

EARTH_CORE_NAMESPACE_BEGIN


// 四叉树节点状态标记
enum
{
	FLAG_HAS_IMAGE = 1 << 0,
	FLAG_HAS_DEM = 1 << 1,
	FLAG_HAS_CULL = 1 << 2,
	FLAG_RENDER = 1 << 3,
};

inline bool HasImage(uint32_t flag)
{
	return (flag & FLAG_HAS_IMAGE) ? true : false;
}

inline bool HasDem(uint32_t flag)
{
	return (flag & FLAG_HAS_DEM) ? true : false;
}

inline bool HasFlag(uint32_t flag, uint32_t checkFlag)
{
	return (flag & checkFlag) ? true : false;
}

inline bool HasNoFlag(uint32_t flag, uint32_t checkFlag)
{
	return !HasFlag(flag, checkFlag);
}

// 瓦片四叉树的定义
class QuadNode
{
public:
	enum ChildRegion
	{
		CHILD_LT,
		CHILD_RT,
		CHILD_LB,
		CHILD_RB,
	};

	using QuadNodePtr = std::shared_ptr<QuadNode>;

	// 经纬度起始和结束点
	Vector2d  mLLStart;
	Vector2d  mLLEnd;

	// 瓦片的世界坐标的包围盒
	AxisAlignedBoxd mBoundingBox;

	/// 位置区域
	ChildRegion  mRegion;
	/// 当前瓦片的父节点
	QuadNode* mParent = nullptr;
	/// 瓦片的孩子节点
	QuadNodePtr mChildNodes[4];

	// 瓦片ID
	QuadTileID mTileID;

	// 状态标记
	uint32_t mStatusFlag = 0;

	QuadNode(QuadNode* parent
		, const Vector2d& vStart
		, const Vector2d& vEnd
		, uint32_t level
		, ChildRegion region
	);
	~QuadNode();

	// 判断是否有子节点
	bool HasChild() const;

	// 四叉树节点的经纬度中心点
	Vector2d GetLonLatCenter() const;
	
	// 四叉树节点的经纬度范围
	Vector2d GetLonLatRange() const;
	
	// 四叉树节点更新
	void Update(const EarthCameraPtr& camera);
};

using QuadTreePtr = QuadNode::QuadNodePtr;

EARTH_CORE_NAMESPACE_END

#endif // !GNX_MAP_ENGINE_QUADTREE_INCLUDE_GJGJDF
