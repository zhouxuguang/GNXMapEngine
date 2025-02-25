#include "QuadTileID.h"

EARTH_CORE_NAMESPACE_BEGIN

QuadTileID GetTileID(uint32_t level, double rLong, double rLat)
{
	// ����ü���x��y�����ܹ��ж�������Ƭ
	uint32_t    xTiles = 2 << level;
	uint32_t    yTiles = 1 << level;

	// ����ÿ����Ƭռ�ݵľ�γ�ȷ�Χ
	double      xWidth = M_PI * 2.0 / xTiles;
	double      yHeight = M_PI / yTiles;

	// ���㾭γ�����ID
	double      xCoord = (rLong + M_PI) / xWidth;
	if (xCoord >= xTiles)
	{
		xCoord = xTiles - 1;
	}

	// ����γ�ȷ����ID
	double      yCoord = (M_PI_2 - rLat) / yHeight;
	if (yCoord >= yTiles)
	{
		yCoord = yTiles - 1;
	}

	return  QuadTileID(level, xCoord, yCoord);
}


EARTH_CORE_NAMESPACE_END