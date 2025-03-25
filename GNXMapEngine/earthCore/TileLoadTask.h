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
#include "BaseLib/BaseLib.h"

EARTH_CORE_NAMESPACE_BEGIN

// 瓦片加载的任务
class TileLoadTask : public baselib::TaskRunner
{
public:
    TileLoadTask();
    ~TileLoadTask();

    virtual void Run();

    LayerBasePtr layer;
    QuadTileID tileId;
};

using TileLoadTaskPtr = std::shared_ptr<TileLoadTask>;

EARTH_CORE_NAMESPACE_END

#endif // GNX_EARTHENGINE_CORE_TILELOAD_TASK_INCLUDE_JKDGDFDFJ