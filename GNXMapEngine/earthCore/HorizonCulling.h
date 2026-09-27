#ifndef GNX_MAP_ENGINE_HORIZON_CULLING_H
#define GNX_MAP_ENGINE_HORIZON_CULLING_H

#include "EarthEngineDefine.h"
#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

class GlobeRectangle;

// Earth-fixed, double-precision ellipsoid horizon culling. The height-aware
// overload uses Cesium's rectangle horizon point and shrinks the occluder for
// negative terrain. The zero-height overload retains the analytic surface
// test for callers that know their geometry lies on the reference ellipsoid.
class HorizonCulling
{
public:
    struct TileBounds
    {
        double west = 0.0;
        double east = 0.0; // 跨日期变更线时可大于 pi
        double westCos = 1.0;
        double westSin = 0.0;
        double eastCos = 1.0;
        double eastSin = 0.0;
        double southCos = 1.0; // 缩放到单位球后的地心纬度
        double southSin = 0.0;
        double northCos = 1.0;
        double northSin = 0.0;
        Vector3d occludeePoint;
        double minimumHeight = 0.0;
        bool hasHeightEnvelope = false;
        bool hasOccludeePoint = false;
    };

    explicit HorizonCulling(const Ellipsoid& ellipsoid);

    TileBounds PrepareTile(const GlobeRectangle& rectangle) const;
    // Cesium-style horizon point for a known height envelope. The envelope must
    // cover every mesh that can replace this tile, including future children.
    TileBounds PrepareTile(const GlobeRectangle& rectangle,
                           double minimumHeight, double maximumHeight) const;
    void SetCameraPosition(const Vector3d& position);
    bool IsOccluded(const TileBounds& tile) const;

private:
    Ellipsoid mEllipsoid;
    Vector3d mInverseRadii;
    Vector3d mCameraPosition;
    Vector3d mCameraScaled;
    double mCameraLongitude = 0.0;
    bool mCameraOutside = false;
};

EARTH_CORE_NAMESPACE_END

#endif
