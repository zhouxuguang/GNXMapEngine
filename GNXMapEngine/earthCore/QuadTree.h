#ifndef GNX_MAP_ENGINE_QUADTREE_INCLUDE_GJGJDF
#define GNX_MAP_ENGINE_QUADTREE_INCLUDE_GJGJDF

#include "EarthEngineDefine.h"
#include "EarthCamera.h"
#include "QuadTileID.h"
#include "DEMMeshData.h"
#include "TileLoadState.h"

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

class EarthNode;

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
	using QuadNodeArray = std::vector<QuadNode*>;

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

	// 渲染相关数据
	DemData mDemData;
	mathutil::Vector3d mStartPoint;   //左下角地理坐标对应的空间直角坐标
	RenderCore::UniformBufferPtr mLocalUniform = nullptr;
	bool mInited = false;
	RenderCore::RCBufferPtr mVertexBuffer = nullptr;
	RenderCore::RCBufferPtr mIndexBuffer = nullptr;
	RenderCore::RCTexturePtr mTexture = nullptr;
	RenderCore::TextureUploadPtr mTextureUpload;

	// 后台瓦片加载结果（由本节点与加载任务共享持有，后台线程只写 CPU 数据）
	TileLoadStatePtr mLoadState = std::make_shared<TileLoadState>();

	QuadNode(EarthNode* earthNode, QuadNode* parent
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

	void GetRenderableNodes(QuadNodeArray& nodes);

	/**
	 * 渲染线程：把后台线程加载好的瓦片数据转成 GPU 资源。
	 * 必须在渲染线程调用（创建纹理并提交异步上传）。
	 */
	void ApplyLoadedTileData();

	/**
	 * 渲染线程：确保顶点/索引缓冲已经创建（未到达 DEM 数据时按平地生成）。
	 */
	void EnsureGpuBuffers();

	/**
	 * GPU 资源是否全部就绪。
	 * 纹理/顶点缓冲/索引缓冲任意一个缺失时提交绘制都是非法操作，
	 * 在部分驱动上会直接导致 VK_ERROR_DEVICE_LOST，因此必须作为渲染的前置条件。
	 */
	bool IsGpuReady() const;

	RenderCore::RCTexturePtr GetTexture() const
	{
		return mTexture;
	}

private:
	EarthNode* mEarthNode = nullptr;
};

using QuadTreePtr = QuadNode::QuadNodePtr;
using QuadNodePtr = QuadTreePtr;

EARTH_CORE_NAMESPACE_END

#endif // !GNX_MAP_ENGINE_QUADTREE_INCLUDE_GJGJDF
