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

struct QuadTreeStats
{
    uint64_t nodesCreated = 0;
    uint64_t nodesDestroyed = 0;
    uint64_t splits = 0;
    uint64_t merges = 0;
    uint64_t requests = 0;
    uint64_t results = 0;
    uint64_t emptyResults = 0;
};

QuadTreeStats& GetQuadTreeStats();

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

	// DEM 更新后需重建顶点缓冲。
	bool mGeometryDirty = true;

	// 后台瓦片加载结果（由本节点与加载任务共享持有，后台线程只写 CPU 数据）
	TileLoadStatePtr mLoadState = std::make_shared<TileLoadState>();

	// 已发任务数与已处理结果数，空结果也计数。
	uint32_t mPendingLayerTasks = 0;
	uint32_t mSettledLayerTasks = 0;

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
	 * 有纹理时创建网格；DEM 更新后重建顶点缓冲。
	 */
	void EnsureGpuBuffers();

	/**
	 * GPU 资源是否全部就绪。
	 * 纹理/顶点缓冲/索引缓冲任意一个缺失时提交绘制都是非法操作，
	 * 在部分驱动上会直接导致 VK_ERROR_DEVICE_LOST，因此必须作为渲染的前置条件。
	 */
	bool IsGpuReady() const;

	// 所有加载任务是否已有结果。
	bool IsLoadSettled() const;

	// GPU 就绪且未被剔除。
	bool IsDrawable() const;

	// 四个子节点均可接替父节点绘制。
	bool AreChildrenFullyReady() const;

	RenderCore::RCTexturePtr GetTexture() const
	{
		return mTexture;
	}

private:
	// 更新视锥剔除标记。
	void UpdateCullFlag(const EarthCameraPtr& camera);

	// 相机距离与瓦片半尺寸之比。
	double ComputeSplitRatio(const EarthCameraPtr& camera) const;

	// 创建四个子节点。
	void CreateChildNodes();

	// 合并子节点。
	void FreeChildNodes();

	EarthNode* mEarthNode = nullptr;
};

using QuadTreePtr = QuadNode::QuadNodePtr;
using QuadNodePtr = QuadTreePtr;

EARTH_CORE_NAMESPACE_END

#endif // !GNX_MAP_ENGINE_QUADTREE_INCLUDE_GJGJDF
