//
//  LayerBase.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_EARTHENGINE_CORE_LAYERBASE_INCLUDE_DKGJJDFGJ
#define GNX_EARTHENGINE_CORE_LAYERBASE_INCLUDE_DKGJJDFGJ

#include "EarthEngineDefine.h"
#include "Ellipsoid.h"
#include "QuadTree.h"
#include "TileDataSource.h"

EARTH_CORE_NAMESPACE_BEGIN

enum LayerType : uint32_t
{
    LT_Image,
    LT_Terrain,
};

//数据图层的基类
class LayerBase
{
private:
    LayerType mLayerType;
    std::string mName;
    // 数据源
    TileDataSourcePtr mDataSourcePtr = nullptr;
public:
    LayerBase(const std::string& name, LayerType type);
    ~LayerBase();

    void setDataSource(TileDataSourcePtr dataSource)
    {
        mDataSourcePtr = dataSource;
    }

    // 创建瓦片加载的任务
    void createTask();

    // 销毁瓦片加载的任务
    void destroyTask();

    // 读取瓦片数据
    bool readTile();
};

EARTH_CORE_NAMESPACE_END

#endif //GNX_EARTHENGINE_CORE_LAYERBASE_INCLUDE_DKGJJDFGJ