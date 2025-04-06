#include "TileLoadTask.h"

EARTH_CORE_NAMESPACE_BEGIN

TileLoadTask::TileLoadTask()
{
}

TileLoadTask::~TileLoadTask()
{
}

// 这里是实际加载数据的逻辑
void TileLoadTask::Run()
{
    ObjectBasePtr tileData = layer->ReadTile(tileId);
    if (tileData)
    {
        // 节点加上有影像的标记
        nodePtr->mStatusFlag |= FLAG_HAS_IMAGE;
    }
}

EARTH_CORE_NAMESPACE_END
