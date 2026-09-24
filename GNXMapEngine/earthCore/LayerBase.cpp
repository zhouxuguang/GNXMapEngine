#include "LayerBase.h"
#include "TileLoadTask.h"

EARTH_CORE_NAMESPACE_BEGIN

LayerBase::LayerBase(const std::string& name, LayerType type)
{
    mName = name;
    mLayerType = type;
}

LayerBase::~LayerBase()
{
    mLoadTiles.clear();
    mLoadedTileData.clear();
}

// 创建瓦片加载的任务
TaskRunnerPtr LayerBase::CreateTask(QuadNode* node, const TileLoadStatePtr& loadState)
{
    if (!node || !loadState)
    {
        return nullptr;
    }
    size_t key = baselib::GetHashCode(node->mTileID);

    // 已经在加载中，不要创建加载任务了
    if (mLoadTiles.find(key) != mLoadTiles.end())
    {
        return  nullptr;
    }

    TileLoadTaskPtr tileLoadTask = std::make_shared<TileLoadTask>();
    tileLoadTask->layer = toPtr<LayerBase>();
    tileLoadTask->tileId = node->mTileID;
    tileLoadTask->loadState = loadState;
    mLoadTiles.insert(key);

    return tileLoadTask;
}

void LayerBase::DestroyTask(const QuadTileID& tileID)
{
    size_t key = baselib::GetHashCode(tileID);
	auto itr = mLoadTiles.find(key);
	if (itr != mLoadTiles.end())
	{
        mLoadTiles.erase(itr);
	}
}

ObjectBasePtr LayerBase::ReadTile(const QuadTileID& tileID)
{
    // 用数据源的读取接口读取数据了
    // 结果由 TileLoadState 直接交给所属节点；不要再放进全局历史数组，
    // 否则每次 zoom 重新加载都会永久保留一份解码图像/DEM 数据。
    return mDataSourcePtr ? mDataSourcePtr->ReadTile(tileID) : nullptr;
}

void LayerBase::SwapLoaedTiles(std::vector<ObjectBasePtr>& loadedTiles)
{
    baselib::AutoLock lockGuard(mTileDataLock);
    loadedTiles = mLoadedTileData;
    mLoadedTileData.clear();
}

EARTH_CORE_NAMESPACE_END
