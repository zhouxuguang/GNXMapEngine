//
//  TileLoadTask.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_EARTHENGINE_CORE_TILELOAD_TASK_INCLUDE_JKDGDFDFJ
#define GNX_EARTHENGINE_CORE_TILELOAD_TASK_INCLUDE_JKDGDFDFJ

#include "QuadTree.h"
#include "LayerBase.h"
#include "Runtime/BaseLib/include/BaseLib.h"

EARTH_CORE_NAMESPACE_BEGIN

// 瓦片加载的任务
class TileLoadTask : public baselib::TaskRunner
{
public:
    TileLoadTask();
    ~TileLoadTask();

    virtual void Run();

    LayerBasePtr layer = nullptr;
    QuadTileID tileId;

    // 只通过共享的加载状态把结果交回渲染线程。
    // 这里不能保存 QuadNode 裸指针：四叉树可能在加载过程中释放节点
    // （zoom out 时 mChildNodes[i] = nullptr），后台线程再写节点就是 use-after-free。
    // 也正因如此，后台线程不能创建任何 GPU 资源。
    TileLoadStatePtr loadState = nullptr;
};

using TileLoadTaskPtr = std::shared_ptr<TileLoadTask>;

EARTH_CORE_NAMESPACE_END

#endif // GNX_EARTHENGINE_CORE_TILELOAD_TASK_INCLUDE_JKDGDFDFJ
