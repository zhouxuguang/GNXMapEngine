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
TaskRunnerPtr LayerBase::CreateTask(QuadNode* node)
{
    if (!node)
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
    tileLoadTask->nodePtr = node;
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
    ObjectBasePtr dataPtr = mDataSourcePtr->ReadTile(tileID);

    {
		baselib::AutoLock lockGuard(mTileDataLock);
		mLoadedTileData.push_back(dataPtr);
    }

    return dataPtr;
}

void LayerBase::SwapLoaedTiles(std::vector<ObjectBasePtr>& loadedTiles)
{
    baselib::AutoLock lockGuard(mTileDataLock);
    loadedTiles = mLoadedTileData;
    mLoadedTileData.clear();
}

EARTH_CORE_NAMESPACE_END