//
//  GeoTransform.cpp
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#include "GeoTransform.h"

EARTH_CORE_NAMESPACE_BEGIN

Matrix4x4d GeoTransform::eastNorthUpToFixedFrame(const Vector3d& origin, const Ellipsoid& ellipsoid) noexcept
{
	if (origin == Vector3d(0.0, 0.0, 0.0)) 
	{
		// 接近地心的位置，退化处理
		return Matrix4x4d(  0.0, -1.0, 0.0,		origin.x,
							1.0, 0.0, 0.0,		origin.y,
							0.0,  0.0, 1.0,		origin.z,
							0.0, 0.0, 0.0,		1.0);
	}
	if (fabs(origin.x - 0.0) < Epsilon14 &&
		fabs(origin.y - 0.0) < Epsilon14)
	{
		// 接近极点的位置，需要特殊处理
		const double sign = Sign(origin.z);
		return Matrix4x4d(
			0, -1.0 * sign, 0,  origin.x,
			1.0, 0.0, 0.0,		origin.y,
			0, 0, 1.0 * sign,	origin.z,
			0, 0, 0,			1.0);
	}

	const Vector3d up = ellipsoid.GeodeticSurfaceNormal(origin);
	const Vector3d east = Vector3d(-origin.y, origin.x, 0.0).Normalize();
	const Vector3d north = Vector3d::CrossProduct(up, east);

	return Matrix4x4d(
		east.x, north.x, up.x, origin.x,
		east.y, north.y, up.y, origin.y,
		east.z, north.z, up.z, origin.z,
		0.0, 0.0, 0.0, 1.0);
}

Matrix4x4d GeoTransform::eastNorthUpToFixedFrame(
    const Geodetic3D& origin,
    const Ellipsoid& ellipsoid) noexcept
{
    Vector3d cartOrigin = ellipsoid.CartographicToCartesian(origin);
    return eastNorthUpToFixedFrame(cartOrigin, ellipsoid);
}

EARTH_CORE_NAMESPACE_END
