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
}

// 创建瓦片加载的任务
TaskRunnerPtr LayerBase::createTask(const QuadTileID& tileID)
{
    size_t key = baselib::GetHashCode(tileID);

    // 已经在加载中，不要创建加载任务了
    if (mLoadTiles.find(key) != mLoadTiles.end())
    {
        return  nullptr;
    }

    TileLoadTaskPtr tileLoadTask = std::make_shared<TileLoadTask>();
    //tileLoadTask->layer = this;
    //tileLoadTask->node = node;
    tileLoadTask->tileId = tileID;
    mLoadTiles.insert(key);

    return tileLoadTask;
}

void LayerBase::destroyTask(const QuadTileID& tileID)
{
    size_t key = baselib::GetHashCode(tileID);
	auto itr = mLoadTiles.find(key);
	if (itr != mLoadTiles.end())
	{
        mLoadTiles.erase(itr);
	}
}

bool LayerBase::readTile(TaskRunner* task)
{
    // 用数据源的读取接口读取数据了
    return false;
}

EARTH_CORE_NAMESPACE_END