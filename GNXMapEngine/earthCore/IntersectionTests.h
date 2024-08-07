//
//  IntersectionTests.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/7/4.
//

#ifndef GNX_MAPENGINE_INTERSECTIONTEST_INCLUDE_SFJDSGH
#define GNX_MAPENGINE_INTERSECTIONTEST_INCLUDE_SFJDSGH

#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

class IntersectionTests
{
public:
    
    // 计算射线与椭球的交点
    static bool RayEllipsoid(const Ray& ray, const Ellipsoid& ellipsoid, Vector3d& intersectPoint) noexcept;
};

EARTH_CORE_NAMESPACE_END

#endif /* GNX_MAPENGINE_INTERSECTIONTEST_INCLUDE_SFJDSGH */
