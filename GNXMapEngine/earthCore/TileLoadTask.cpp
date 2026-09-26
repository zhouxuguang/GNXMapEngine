#include "TileLoadTask.h"
#include "TiledImage.h"
#include "Runtime/BaseLib/include/LogService.h"

EARTH_CORE_NAMESPACE_BEGIN

TileLoadTask::TileLoadTask()
{
}

TileLoadTask::~TileLoadTask()
{
}

// 这里是实际加载数据的逻辑
//
// 注意：本函数运行在后台加载线程上，只能做 CPU 解码，不能触碰 QuadNode，
// 也不能调用任何渲染接口（创建纹理 / 提交命令缓冲区）。GPU 资源统一由
// 渲染线程在 QuadNode::Update -> ApplyLoadedTileData 中创建。
void TileLoadTask::Run()
{
    if (!layer || !loadState)
    {
        return;
    }

    ObjectBasePtr tileData = layer->ReadTile(tileId);

    // 无数据时也回传空结果，供节点结算任务。
    loadState->SetLoadedData(tileData, layer->GetLayerType() == LayerType::LT_Terrain);
}

EARTH_CORE_NAMESPACE_END
