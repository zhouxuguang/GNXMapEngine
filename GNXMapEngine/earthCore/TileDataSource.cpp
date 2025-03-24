#include "TileDataSource.h"

EARTH_CORE_NAMESPACE_BEGIN

TileDataSource::TileDataSource(const std::string& dataPath, const std::string& extName)
{
    mDataPath = dataPath;
    mExtName = extName;
}

TileDataSource::~TileDataSource()
{
}

// 读取tile的数据
bool TileDataSource::readTile()
{
    return true;
}

EARTH_CORE_NAMESPACE_END