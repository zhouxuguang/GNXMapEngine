#include "HorizonCulling.h"

#include "Ellipsoid.h"
#include "GlobeRectangle.h"

#include <algorithm>
#include <cmath>

EARTH_CORE_NAMESPACE_BEGIN

namespace
{
constexpr double kTwoPi = 2.0 * M_PI;
// 在切线附近保留一个很小的可见带，避免浮点舍入导致误剔除。
constexpr double kVisibilityMargin = 1.0e-12;

void ScaledLatitude(double geodeticLatitude, double polarOverEquatorial,
                    double& cosine, double& sine)
{
    // 对旋转椭球，缩放空间中的地心纬度满足 tan(beta)=(b/a)tan(phi)。
    const double x = std::cos(geodeticLatitude);
    const double z = polarOverEquatorial * std::sin(geodeticLatitude);
    const double length = std::hypot(x, z);
    cosine = x / length;
    sine = z / length;
}
} // namespace

HorizonCulling::HorizonCulling(const Ellipsoid& ellipsoid)
    : mInverseRadii(ellipsoid.GetOneOverRadii())
{
}

HorizonCulling::TileBounds HorizonCulling::PrepareTile(const GlobeRectangle& rectangle) const
{
    TileBounds tile;
    tile.west = rectangle.getWest();
    tile.east = rectangle.getEast();
    if (tile.east < tile.west)
        tile.east += kTwoPi;

    tile.westCos = std::cos(tile.west);
    tile.westSin = std::sin(tile.west);
    tile.eastCos = std::cos(tile.east);
    tile.eastSin = std::sin(tile.east);

    const double equatorialRadius = 1.0 / mInverseRadii.x;
    const double polarOverEquatorial = (1.0 / mInverseRadii.z) / equatorialRadius;
    ScaledLatitude(rectangle.getSouth(), polarOverEquatorial, tile.southCos, tile.southSin);
    ScaledLatitude(rectangle.getNorth(), polarOverEquatorial, tile.northCos, tile.northSin);
    return tile;
}

void HorizonCulling::SetCameraPosition(const Vector3d& position)
{
    mCameraScaled = Vector3d(position.x * mInverseRadii.x,
                             position.y * mInverseRadii.y,
                             position.z * mInverseRadii.z);
    const double magnitudeSquared = mCameraScaled.DotProduct(mCameraScaled);
    mCameraOutside = std::isfinite(magnitudeSquared) && magnitudeSquared > 1.0;
    mCameraLongitude = std::atan2(mCameraScaled.y, mCameraScaled.x);
}

bool HorizonCulling::IsOccluded(const TileBounds& tile) const
{
    // 在椭球上或内部时没有有效的切线锥，保守地保留所有瓦片。
    if (!mCameraOutside)
        return false;

    const double cx = mCameraScaled.x;
    const double cy = mCameraScaled.y;
    const double cz = mCameraScaled.z;

    // max_lambda(cx*cos(lambda)+cy*sin(lambda))。
    double horizontal = std::max(cx * tile.westCos + cy * tile.westSin,
                                 cx * tile.eastCos + cy * tile.eastSin);
    if (tile.east - tile.west >= kTwoPi)
    {
        horizontal = std::hypot(cx, cy);
    }
    else
    {
        double eyeLongitude = mCameraLongitude;
        if (eyeLongitude < tile.west)
            eyeLongitude += kTwoPi;
        if (eyeLongitude >= tile.west && eyeLongitude <= tile.east)
            horizontal = std::hypot(cx, cy);
    }

    // max_beta(horizontal*cos(beta)+cz*sin(beta))。
    double maximum = std::max(horizontal * tile.southCos + cz * tile.southSin,
                              horizontal * tile.northCos + cz * tile.northSin);
    const double southCross = tile.southCos * cz - tile.southSin * horizontal;
    const double northCross = horizontal * tile.northSin - cz * tile.northCos;
    if (southCross >= 0.0 && northCross >= 0.0)
        maximum = std::max(maximum, std::hypot(horizontal, cz));

    return std::isfinite(maximum) && maximum < 1.0 - kVisibilityMargin;
}

EARTH_CORE_NAMESPACE_END
