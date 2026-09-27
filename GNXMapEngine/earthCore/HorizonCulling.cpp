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
    : mEllipsoid(ellipsoid), mInverseRadii(ellipsoid.GetOneOverRadii())
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

HorizonCulling::TileBounds HorizonCulling::PrepareTile(
    const GlobeRectangle& rectangle, double minimumHeight, double maximumHeight) const
{
    TileBounds tile = PrepareTile(rectangle);
    tile.minimumHeight = minimumHeight;
    tile.hasHeightEnvelope = true;
    const Vector3d radii = mEllipsoid.GetAxis();
    const double shrink = std::min(0.0, minimumHeight);
    const Vector3d scaledRadii(radii.x + shrink, radii.y + shrink, radii.z + shrink);
    if (!std::isfinite(minimumHeight) || !std::isfinite(maximumHeight) ||
        minimumHeight > maximumHeight || scaledRadii.x <= 0.0 ||
        scaledRadii.y <= 0.0 || scaledRadii.z <= 0.0)
        return tile;

    const double centerLongitude = (tile.west + tile.east) * 0.5;
    const double centerLatitude = (rectangle.getSouth() + rectangle.getNorth()) * 0.5;
    const Vector3d center = mEllipsoid.CartographicToCartesian(
        Geodetic3D(centerLongitude, centerLatitude, maximumHeight));
    Vector3d direction(center.x / scaledRadii.x,
                       center.y / scaledRadii.y,
                       center.z / scaledRadii.z);
    const double directionLength = direction.Length();
    if (!(directionLength > 0.0) || !std::isfinite(directionLength))
        return tile;
    direction = direction / directionLength;

    // Same horizon-point construction as Cesium's
    // computeHorizonCullingPointPossiblyUnderEllipsoid. For a rectangle, Cesium
    // uses its four corners at maximum height when a mesh point is unavailable.
    double largestMagnitude = 0.0;
    for (const double longitude : {tile.west, tile.east})
    {
        for (const double latitude : {rectangle.getSouth(), rectangle.getNorth()})
        {
            const Vector3d world = mEllipsoid.CartographicToCartesian(
                Geodetic3D(longitude, latitude, maximumHeight));
            const Vector3d point(world.x / scaledRadii.x,
                                 world.y / scaledRadii.y,
                                 world.z / scaledRadii.z);
            const double length = point.Length();
            if (!(length > 0.0) || !std::isfinite(length))
                return tile;
            const double cosAlpha = std::clamp(point.DotProduct(direction) / length, -1.0, 1.0);
            const double sinAlpha = std::sqrt(std::max(0.0, 1.0 - cosAlpha * cosAlpha));
            const double adjustedLength = std::max(1.0, length);
            const double denominator = (cosAlpha -
                sinAlpha * std::sqrt(adjustedLength * adjustedLength - 1.0)) /
                adjustedLength;
            if (!(denominator > 0.0) || !std::isfinite(denominator))
                return tile;
            largestMagnitude = std::max(largestMagnitude, 1.0 / denominator);
        }
    }
    if (!std::isfinite(largestMagnitude))
        return tile;
    tile.occludeePoint = direction * largestMagnitude;
    tile.hasOccludeePoint = true;
    return tile;
}

void HorizonCulling::SetCameraPosition(const Vector3d& position)
{
    mCameraPosition = position;
    mCameraScaled = Vector3d(position.x * mInverseRadii.x,
                             position.y * mInverseRadii.y,
                             position.z * mInverseRadii.z);
    const double magnitudeSquared = mCameraScaled.DotProduct(mCameraScaled);
    mCameraOutside = std::isfinite(magnitudeSquared) && magnitudeSquared > 1.0;
    mCameraLongitude = std::atan2(mCameraScaled.y, mCameraScaled.x);
}

bool HorizonCulling::IsOccluded(const TileBounds& tile) const
{
    if (tile.hasHeightEnvelope && !tile.hasOccludeePoint)
        return false;
    if (tile.hasOccludeePoint)
    {
        const Vector3d radii = mEllipsoid.GetAxis();
        const double shrink = std::min(0.0, tile.minimumHeight);
        const Vector3d camera(mCameraPosition.x / (radii.x + shrink),
                              mCameraPosition.y / (radii.y + shrink),
                              mCameraPosition.z / (radii.z + shrink));
        const double limbSquared = camera.DotProduct(camera) - 1.0;
        if (!(limbSquared > 0.0) || !std::isfinite(limbSquared))
            return false;
        const Vector3d toPoint = tile.occludeePoint - camera;
        const double distanceSquared = toPoint.DotProduct(toPoint);
        const double towardCenter = -toPoint.DotProduct(camera);
        // Cesium's plane and cone checks, with a small tangent safety margin.
        return distanceSquared > 0.0 &&
            towardCenter > limbSquared + kVisibilityMargin &&
            towardCenter * towardCenter / distanceSquared >
                limbSquared + kVisibilityMargin;
    }

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
