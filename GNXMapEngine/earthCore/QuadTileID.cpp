#include "QuadTileID.h"

EARTH_CORE_NAMESPACE_BEGIN

QuadTileID GetTileID(uint32_t level, double rLong, double rLat)
{
	// 计算该级别x和y方向总共有多少张瓦片
	uint32_t    xTiles = 2 << level;
	uint32_t    yTiles = 1 << level;

	// 计算每张瓦片占据的经纬度范围
	double      xWidth = M_PI * 2.0 / xTiles;
	double      yHeight = M_PI / yTiles;

	// 计算经纬方向的ID
	double      xCoord = (rLong + M_PI) / xWidth;
	if (xCoord >= xTiles)
	{
		xCoord = xTiles - 1;
	}

	// 计算纬度方向的ID
	double      yCoord = (M_PI_2 - rLat) / yHeight;
	if (yCoord >= yTiles)
	{
		yCoord = yTiles - 1;
	}

	return  QuadTileID(level, xCoord, yCoord);
}


EARTH_CORE_NAMESPACE_END