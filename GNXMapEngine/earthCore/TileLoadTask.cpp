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
    layer->ReadTile(tileId);
}

EARTH_CORE_NAMESPACE_END
