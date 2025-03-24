//
//  TileDataSource.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_EARTHENGINE_CORE_TILEDATASOURCE_INCLUDE_KJDFDFJNBJ
#define GNX_EARTHENGINE_CORE_TILEDATASOURCE_INCLUDE_KJDFDFJNBJ

#include "EarthEngineDefine.h"
#include "Ellipsoid.h"
#include "QuadTree.h"

EARTH_CORE_NAMESPACE_BEGIN

// 数据源对象(瓦片对象实际加载的数据)
class TileDataSource
{
private:
    std::string mDataPath;
    std::string mExtName;
public:
    TileDataSource(const std::string& dataPath, const std::string& extName);
    ~TileDataSource();

    bool readTile();
};

using TileDataSourcePtr = std::shared_ptr<TileDataSource>;

EARTH_CORE_NAMESPACE_END

#endif