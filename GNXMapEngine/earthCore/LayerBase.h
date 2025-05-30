//
//  LayerBase.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_EARTHENGINE_CORE_LAYERBASE_INCLUDE_DKGJJDFGJ
#define GNX_EARTHENGINE_CORE_LAYERBASE_INCLUDE_DKGJJDFGJ

#include "QuadTree.h"
#include "TileDataSource.h"
#include "ObjectBase.h"

EARTH_CORE_NAMESPACE_BEGIN

enum LayerType : uint32_t
{
    LT_Image,
    LT_Terrain,
};

//数据图层的基类
class LayerBase : public ObjectBase
{
private:
    LayerType mLayerType;
    std::string mName;
    // 数据源
    TileDataSourcePtr mDataSourcePtr = nullptr;
    std::set<size_t> mLoadTiles;   // 当前图层加载的任务ID，用于去重，防止同一个数据重复加载，浪费资源

    std::vector<ObjectBasePtr> mLoadedTileData;   //已经加载完成的瓦片数据
    baselib::MutexLock mTileDataLock;

public:
    LayerBase(const std::string& name, LayerType type);
    ~LayerBase();

    /**
     * 设置数据源
     */
    void SetDataSource(TileDataSourcePtr dataSource)
    {
        mDataSourcePtr = dataSource;
    }

    /**
     * 创建瓦片加载的任务
     */
    TaskRunnerPtr CreateTask(QuadNode* node);

    /**
     * 销毁瓦片加载的任务
     */
    void DestroyTask(const QuadTileID& tileID);

    /**
     * 读取瓦片数据
     */
    ObjectBasePtr ReadTile(const QuadTileID& tileID);

    /**
     * 交换加载好的数据
     */
    void SwapLoaedTiles(std::vector<ObjectBasePtr>& loadedTiles);

    /**
     * 获得当前图层的名字
     */
    const std::string& GetLayerName() const
    {
        return mName;
    }

    /**
     * 获得当前图层的名字
     */
    std::string& GetLayerName()
    {
        return mName;
    }

    /**
     * 获得图层的类型
     */
    LayerType GetLayerType()
	{
		return mLayerType;
	}
};

using LayerBasePtr = std::shared_ptr<LayerBase>;

EARTH_CORE_NAMESPACE_END

#endif //GNX_EARTHENGINE_CORE_LAYERBASE_INCLUDE_DKGJJDFGJ