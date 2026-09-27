#ifndef GNX_MAP_ENGINE_HORIZON_CULLING_H
#define GNX_MAP_ENGINE_HORIZON_CULLING_H

#include "EarthEngineDefine.h"

EARTH_CORE_NAMESPACE_BEGIN

class Ellipsoid;
class GlobeRectangle;

// WGS84 椭球面瓦片的精确地平线测试。所有坐标均为地心固定坐标系中的 double。
// 把椭球缩放为单位球后，表面点 P 从相机 C 可见当且仅当 C·P >= 1。
// 对经纬度矩形求 C·P 的最大值，因此不会仅因瓦片中心被遮挡而误剔除边缘。
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
    };

    explicit HorizonCulling(const Ellipsoid& ellipsoid);

    TileBounds PrepareTile(const GlobeRectangle& rectangle) const;
    void SetCameraPosition(const Vector3d& position);
    bool IsOccluded(const TileBounds& tile) const;

private:
    Vector3d mInverseRadii;
    Vector3d mCameraScaled;
    double mCameraLongitude = 0.0;
    bool mCameraOutside = false;
};

EARTH_CORE_NAMESPACE_END

#endif
