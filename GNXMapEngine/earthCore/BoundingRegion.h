//
//  BoundingRegion.h
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright  2018 年 Zhou,Xuguang. All rights reserved.
//

#ifndef GNX_MAPENGINE_BOUNDING_REGION_INCLUDE_SGKDFGKFKGKF
#define GNX_MAPENGINE_BOUNDING_REGION_INCLUDE_SGKDFGKFKGKF

#include "EarthEngineDefine.h"
#include "MathUtil/MathUtil.h"
#include "GlobeRectangle.h"
#include "Ellipsoid.h"
#include "rendersystem/OBB.h"
#include "rendersystem/Plane.h"

EARTH_CORE_NAMESPACE_BEGIN

/**
 * @brief 地理区域范围
 */
class BoundingRegion 
{
public:
	/**
	* @brief 构造一个新的地理区域范围
	*
	* @param rectangle The bounding rectangle of the region.
	* @param minimumHeight The minimum height in meters.
	* @param maximumHeight The maximum height in meters.
	* @param ellipsoid The ellipsoid on which this region is defined.
	*/
	BoundingRegion(
		const GlobeRectangle& rectangle,
		double minimumHeight,
		double maximumHeight,
		const Ellipsoid& ellipsoid);

	/**
	* @brief Gets the bounding rectangle of the region.
	*/
	const GlobeRectangle& getRectangle() const noexcept 
	{
		return this->mRectangle;
	}

	/**
	* @brief Gets the minimum height of the region.
	*/
	double getMinimumHeight() const noexcept { return this->mMinimumHeight; }

	/**
	* @brief Gets the maximum height of the region.
	*/
	double getMaximumHeight() const noexcept { return this->mMaximumHeight; }

	/**
	* @brief Gets an oriented bounding box containing this region.
	*/
	const OrientedBoundingBoxd& getBoundingBox() const noexcept 
	{
		return this->mBoundingBox;
	}

private:
	static OrientedBoundingBoxd computeBoundingBox(
		const GlobeRectangle& rectangle,
		double minimumHeight,
		double maximumHeight,
		const Ellipsoid& ellipsoid);

	GlobeRectangle mRectangle;
	double mMinimumHeight;
	double mMaximumHeight;
	OrientedBoundingBoxd mBoundingBox;

	Vector3d mSouthwestCornerCartesian;
	Vector3d mNortheastCornerCartesian;
	Vector3d mWestNormal;
	Vector3d mEastNormal;
	Vector3d mSouthNormal;
	Vector3d mNorthNormal;
	bool mPlanesAreInvalid;
};

EARTH_CORE_NAMESPACE_END

#endif