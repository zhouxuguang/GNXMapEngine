#include "earthCore/HorizonCulling.h"
#include "earthCore/BoundingRegion.h"
#include "earthCore/DEMMeshData.h"
#include "earthCore/EarthCameraPose.h"
#include "earthCore/Ellipsoid.h"
#include "earthCore/Geodetic3D.h"
#include "earthCore/GlobeRectangle.h"
#include "earthCore/QuadTileID.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <random>

using namespace earthcore;

namespace
{
constexpr double kRad = M_PI / 180.0;
int failures = 0;

void Check(const char* label, bool condition)
{
    if (!condition)
    {
        std::printf("FAIL: %s\n", label);
        ++failures;
    }
}

// 独立参考：线段 C->P 是否先与单位球相交。P 是椭球面上的点。
bool SampleVisible(const Ellipsoid& ellipsoid, const Vector3d& eye,
                   double longitude, double latitude)
{
    const Vector3d inverse = ellipsoid.GetOneOverRadii();
    const Vector3d world = ellipsoid.CartographicToCartesian(Geodetic3D(longitude, latitude, 0.0));
    const Vector3d c(eye.x * inverse.x, eye.y * inverse.y, eye.z * inverse.z);
    const Vector3d p(world.x * inverse.x, world.y * inverse.y, world.z * inverse.z);
    const Vector3d v = p - c;
    const double a = v.DotProduct(v);
    const double b = 2.0 * c.DotProduct(v);
    const double d = b * b - 4.0 * a * (c.DotProduct(c) - 1.0);
    if (d <= 0.0)
        return true;
    const double firstHit = (-b - std::sqrt(d)) / (2.0 * a);
    return firstHit >= 1.0 - 1.0e-10 || firstHit <= 0.0;
}

bool SampleTerrainVisible(const Ellipsoid& ellipsoid, const Vector3d& eye,
                          double longitude, double latitude, double height,
                          double minimumHeight)
{
    const Vector3d axes = ellipsoid.GetAxis();
    const double shrink = std::min(0.0, minimumHeight);
    const Vector3d world = ellipsoid.CartographicToCartesian(
        Geodetic3D(longitude, latitude, height));
    const Vector3d c(eye.x / (axes.x + shrink),
                     eye.y / (axes.y + shrink),
                     eye.z / (axes.z + shrink));
    const Vector3d p(world.x / (axes.x + shrink),
                     world.y / (axes.y + shrink),
                     world.z / (axes.z + shrink));
    const Vector3d v = p - c;
    const double a = v.DotProduct(v);
    const double b = 2.0 * c.DotProduct(v);
    const double discriminant = b * b - 4.0 * a * (c.DotProduct(c) - 1.0);
    if (discriminant <= 0.0)
        return true;
    const double firstHit = (-b - std::sqrt(discriminant)) / (2.0 * a);
    return firstHit <= 0.0 || firstHit >= 1.0 - 1.0e-10;
}

bool Contains(const OrientedBoundingBoxd& box, const Vector3d& point)
{
    const Vector3d offset = point - box.mCenter;
    for (int axis = 0; axis < 3; ++axis)
    {
        const Vector3d halfAxis = box.mHalfAxes.col(axis);
        const double length = halfAxis.Length();
        if (!(length > 0.0) ||
            std::abs(offset.DotProduct(halfAxis) / length) > length + 1.0)
            return false;
    }
    return true;
}

struct Counts
{
    int requested = 0;
    int imageFiles = 0;
    int terrainFiles = 0;
    int frustumCulled = 0;
    int horizonCulled = 0;
};

struct DataRoots
{
    std::filesystem::path image;
    std::filesystem::path terrain;
};

bool HasTileFile(const std::filesystem::path& root, const QuadTileID& tile,
                 const char* extension)
{
    if (root.empty())
        return false;
    const std::filesystem::path path = root / std::to_string(tile.level) /
        std::to_string(tile.x) / (std::to_string(tile.y) + extension);
    if (std::filesystem::exists(path))
        return true;
    // TileDataSource 的图像读取还有一个双点文件名回退。
    if (std::string(extension) == ".jpg")
        return std::filesystem::exists(root / std::to_string(tile.level) /
            std::to_string(tile.x) / (std::to_string(tile.y) + "..jpg"));
    return false;
}

void Visit(const Ellipsoid& ellipsoid, const GlobeRectangle& rectangle, int level,
           const Vector3d& eye, const Frustumd& frustum, const HorizonCulling& horizon,
           bool enabled, const DataRoots& roots, Counts& counts)
{
    ++counts.requested; // QuadNode 构造时立即调用 EarthNode::RequestTile。
    const QuadTileID tileId = GetTileID(level,
        (rectangle.getWest() + rectangle.getEast()) * 0.5,
        (rectangle.getSouth() + rectangle.getNorth()) * 0.5);
    counts.imageFiles += HasTileFile(roots.image, tileId, ".jpg") ? 1 : 0;
    counts.terrainFiles += HasTileFile(roots.terrain, tileId, ".terrain") ? 1 : 0;
    const BoundingRegion region(rectangle, DEM_MIN_POSSIBLE_HEIGHT,
                                DEM_MAX_POSSIBLE_HEIGHT, ellipsoid);
    const AxisAlignedBoxd box = region.getBoundingBox().ToAxisAligned();
    if (!frustum.IsBoxInFrustum(box))
    {
        ++counts.frustumCulled;
        return;
    }
    if (enabled && horizon.IsOccluded(horizon.PrepareTile(rectangle,
            DEM_MIN_POSSIBLE_HEIGHT, DEM_MAX_POSSIBLE_HEIGHT)))
    {
        ++counts.horizonCulled;
        return;
    }

    const double radius = (box.maximum - box.minimum).Length() * 0.5;
    const double ratio = (box.center - eye).Length() / radius;
    if (level >= 9 || ratio >= 1.0)
        return;

    const double west = rectangle.getWest();
    const double east = rectangle.getEast();
    const double south = rectangle.getSouth();
    const double north = rectangle.getNorth();
    const double middleLon = (west + east) * 0.5;
    const double middleLat = (south + north) * 0.5;
    Visit(ellipsoid, GlobeRectangle(west, middleLat, middleLon, north), level + 1,
          eye, frustum, horizon, enabled, roots, counts);
    Visit(ellipsoid, GlobeRectangle(middleLon, middleLat, east, north), level + 1,
          eye, frustum, horizon, enabled, roots, counts);
    Visit(ellipsoid, GlobeRectangle(west, south, middleLon, middleLat), level + 1,
          eye, frustum, horizon, enabled, roots, counts);
    Visit(ellipsoid, GlobeRectangle(middleLon, south, east, middleLat), level + 1,
          eye, frustum, horizon, enabled, roots, counts);
}

Counts CountRequests(const Ellipsoid& ellipsoid, const Vector3d& eye,
                     const Vector3d& target, bool enabled, const DataRoots& roots)
{
    const Vector3d up = EarthCameraPose::LookUpForLookAt(eye, target, ellipsoid, 0.0);
    // 与 EarthCamera / QuadNode 一致：double 求视图，float 提交 VP，再转 double 提取视锥。
    const Matrix4x4d preciseView = Matrix4x4d::CreateLookAt(eye, target, up);
    Matrix4x4f view;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            view[row][column] = static_cast<float>(preciseView[row][column]);
    const Matrix4x4f projection = Matrix4x4f::CreateInfiniteReverseZPerspective(60.0f, 16.0f / 9.0f, 10.0f);
    const Matrix4x4f floatViewProjection = projection * view;
    Matrix4x4d viewProjection;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            viewProjection[row][column] = floatViewProjection[row][column];
    Frustumd frustum;
    frustum.InitFrustum(viewProjection);
    HorizonCulling horizon(ellipsoid);
    horizon.SetCameraPosition(eye);
    Counts counts;
    Visit(ellipsoid, GlobeRectangle(-M_PI, -M_PI_2, 0.0, M_PI_2), 0,
          eye, frustum, horizon, enabled, roots, counts);
    Visit(ellipsoid, GlobeRectangle(0.0, -M_PI_2, M_PI, M_PI_2), 0,
          eye, frustum, horizon, enabled, roots, counts);
    return counts;
}
} // namespace

int main()
{
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84;
    DataRoots roots;
    if (const char* path = std::getenv("GNX_MAP_IMAGE_TILES"))
        roots.image = path;
    else
        roots.image = "/Users/zhouxuguang/work/data/gis/tile/image";
    if (const char* path = std::getenv("GNX_MAP_TERRAIN_TILES"))
        roots.terrain = path;
    else
        roots.terrain = "/Users/zhouxuguang/work/data/gis/tile/terrain";
    HorizonCulling horizon(ellipsoid);
    std::mt19937_64 generator(0x3729a4ULL);
    std::uniform_real_distribution<double> longitude(-M_PI, M_PI);
    std::uniform_real_distribution<double> latitude(-M_PI_2, M_PI_2);
    std::uniform_real_distribution<double> tileSpan(0.0001, 1.5);
    std::uniform_real_distribution<double> altitude(20.0, 30000000.0);
    int sampledVisibleInCulledTile = 0;

    for (int i = 0; i < 4000; ++i)
    {
        const double eyeLon = longitude(generator);
        const double eyeLat = latitude(generator);
        const Vector3d eye = ellipsoid.CartographicToCartesian(Geodetic3D(eyeLon, eyeLat, altitude(generator)));
        horizon.SetCameraPosition(eye);
        const double west = longitude(generator);
        const double south = latitude(generator);
        const double east = std::min(M_PI, west + tileSpan(generator));
        const double north = std::min(M_PI_2, south + tileSpan(generator));
        const GlobeRectangle tile(west, south, east, north);
        if (!horizon.IsOccluded(horizon.PrepareTile(tile)))
            continue;
        for (int y = 0; y <= 8; ++y)
            for (int x = 0; x <= 8; ++x)
            {
                const double lon = west + (east - west) * x / 8.0;
                const double lat = south + (north - south) * y / 8.0;
                if (SampleVisible(ellipsoid, eye, lon, lat))
                    ++sampledVisibleInCulledTile;
            }
    }
    Check("4000 random camera/tile cases: no visible sample in culled tile", sampledVisibleInCulledTile == 0);

    // 跨日期变更线、近地平线、极区及整球根瓦片。
    const Vector3d equatorEye = ellipsoid.CartographicToCartesian(Geodetic3D(179.0 * kRad, 0.0, 1000000.0));
    horizon.SetCameraPosition(equatorEye);
    Check("dateline-near tile visible", !horizon.IsOccluded(horizon.PrepareTile(
          GlobeRectangle(170.0 * kRad, -5.0 * kRad, -170.0 * kRad, 5.0 * kRad))));
    Check("opposite tile occluded", horizon.IsOccluded(horizon.PrepareTile(
          GlobeRectangle(-10.0 * kRad, -5.0 * kRad, 10.0 * kRad, 5.0 * kRad))));
    Check("half-globe root remains visible", !horizon.IsOccluded(horizon.PrepareTile(
          GlobeRectangle(-M_PI, -M_PI_2, 0.0, M_PI_2))));

    const Vector3d tangentEye = ellipsoid.CartographicToCartesian(Geodetic3D(0.0, 0.0, 1000000.0));
    horizon.SetCameraPosition(tangentEye);
    const double tangentLongitude = std::acos(ellipsoid.GetAxis().x /
                                                (ellipsoid.GetAxis().x + 1000000.0));
    Check("tile wholly past exact tangent is culled", horizon.IsOccluded(horizon.PrepareTile(
          GlobeRectangle(tangentLongitude + 0.001, -0.001,
                         tangentLongitude + 0.01, 0.001))));
    Check("tile straddling exact tangent is kept", !horizon.IsOccluded(horizon.PrepareTile(
          GlobeRectangle(tangentLongitude - 0.001, -0.001,
                         tangentLongitude + 0.01, 0.001))));
    Check("tile touching exact tangent is kept", !horizon.IsOccluded(horizon.PrepareTile(
          GlobeRectangle(tangentLongitude, 0.0, tangentLongitude + 0.01, 0.001))));

    const Vector3d poleEye = ellipsoid.CartographicToCartesian(
        Geodetic3D(0.0, 89.0 * kRad, 1000000.0));
    horizon.SetCameraPosition(poleEye);
    Check("north polar tile is visible", !horizon.IsOccluded(horizon.PrepareTile(
          GlobeRectangle(-M_PI, 85.0 * kRad, M_PI, M_PI_2))));
    Check("south polar tile is hidden", horizon.IsOccluded(horizon.PrepareTile(
          GlobeRectangle(-M_PI, -M_PI_2, M_PI, -85.0 * kRad))));

    // A zero-height tile is hidden here, while its raised or depressed terrain
    // is visible. The min-height case requires Cesium's shrunken occluder.
    const Vector3d lowEye = ellipsoid.CartographicToCartesian(
        Geodetic3D(0.0, 0.0, 1000.0));
    const GlobeRectangle nearLimb(1.45 * kRad, -0.05 * kRad,
                                  1.55 * kRad, 0.05 * kRad);
    horizon.SetCameraPosition(lowEye);
    Check("zero-height tile is behind horizon",
          horizon.IsOccluded(horizon.PrepareTile(nearLimb)));
    Check("raised terrain has a clear line of sight",
          SampleTerrainVisible(ellipsoid, lowEye, 1.5 * kRad, 0.0, 3000.0, 0.0));
    Check("raised terrain must not be culled",
          !horizon.IsOccluded(horizon.PrepareTile(nearLimb, 3000.0, 3000.0)));
    Check("depressed terrain visible against shrunken ellipsoid",
          SampleTerrainVisible(ellipsoid, lowEye, 1.5 * kRad, 0.0, -3000.0, -3000.0));
    Check("depressed terrain must not be culled",
          !horizon.IsOccluded(horizon.PrepareTile(nearLimb, -3000.0, -3000.0)));
    Check("unknown descendants retain visible terrain",
          !horizon.IsOccluded(horizon.PrepareTile(nearLimb,
              DEM_MIN_POSSIBLE_HEIGHT, DEM_MAX_POSSIBLE_HEIGHT)));

    int visibleTerrainInCulledTile = 0;
    std::uniform_real_distribution<double> terrainHeight(-6000.0, 8000.0);
    for (int i = 0; i < 2000; ++i)
    {
        const Vector3d eye = ellipsoid.CartographicToCartesian(Geodetic3D(
            longitude(generator), latitude(generator), altitude(generator)));
        horizon.SetCameraPosition(eye);
        const double west = longitude(generator);
        const double south = latitude(generator);
        const double east = std::min(M_PI, west + tileSpan(generator));
        const double north = std::min(M_PI_2, south + tileSpan(generator));
        const double sampledMinimum = terrainHeight(generator);
        const double minHeight = i % 2 == 0
            ? DEM_MIN_POSSIBLE_HEIGHT : sampledMinimum;
        const double maxHeight = i % 2 == 0
            ? DEM_MAX_POSSIBLE_HEIGHT
            : std::max(minHeight, terrainHeight(generator));
        const GlobeRectangle tile(west, south, east, north);
        if (!horizon.IsOccluded(horizon.PrepareTile(tile, minHeight, maxHeight)))
            continue;
        for (int y = 0; y <= 6; ++y)
            for (int x = 0; x <= 6; ++x)
                for (const double height : {minHeight, maxHeight})
                    if (SampleTerrainVisible(ellipsoid, eye,
                        west + (east - west) * x / 6.0,
                        south + (north - south) * y / 6.0,
                        height, minHeight))
                        ++visibleTerrainInCulledTile;
    }
    Check("2000 random height envelopes: no visible sample in culled tile",
          visibleTerrainInCulledTile == 0);

    int terrainOutsideTraversalBox = 0;
    for (int level = 0; level <= 9; ++level)
    {
        const double width = M_PI / (1 << level);
        const double height = M_PI / (1 << level);
        for (int i = 0; i < 100; ++i)
        {
            const double west = -M_PI +
                std::floor((longitude(generator) + M_PI) / width) * width;
            const double south = -M_PI_2 +
                std::floor((latitude(generator) + M_PI_2) / height) * height;
            const GlobeRectangle tile(west, south, west + width, south + height);
            const OrientedBoundingBoxd box = BoundingRegion(tile,
                DEM_MIN_POSSIBLE_HEIGHT, DEM_MAX_POSSIBLE_HEIGHT,
                ellipsoid).getBoundingBox();
            for (int y = 0; y <= 4; ++y)
                for (int x = 0; x <= 4; ++x)
                    for (const double sampleHeight : {DEM_MIN_POSSIBLE_HEIGHT,
                                                       0.0, DEM_MAX_POSSIBLE_HEIGHT})
                        if (!Contains(box, ellipsoid.CartographicToCartesian(
                            Geodetic3D(west + width * x / 4.0,
                                       south + height * y / 4.0, sampleHeight))))
                            ++terrainOutsideTraversalBox;
        }
    }
    Check("terrain envelope OBB contains sampled future meshes",
          terrainOutsideTraversalBox == 0);

    struct Scenario { const char* name; double eyeLon, eyeLat, altitude, targetLon, targetLat; };
    const Scenario cases[] = {
        {"orbital nadir", 0, 0, 6400000, 0, 0},
        {"orbital oblique", 0, 0, 6400000, 55, 0},
        {"mid altitude", 110, 23, 500000, 110, 23},
        {"mid oblique", 110, 23, 500000, 120, 23},
        {"low altitude", 110, 23, 10000, 110, 23},
        {"low oblique", 110, 23, 10000, 110.5, 23},
        {"high latitude", 40, 78, 500000, 40, 78},
    };
    std::puts("scenario             requested off/on  image files off/on  terrain files off/on  horizon rejected");
    for (const Scenario& scenario : cases)
    {
        const Vector3d eye = ellipsoid.CartographicToCartesian(Geodetic3D(
            scenario.eyeLon * kRad, scenario.eyeLat * kRad, scenario.altitude));
        const Vector3d target = ellipsoid.CartographicToCartesian(Geodetic3D(
            scenario.targetLon * kRad, scenario.targetLat * kRad, 0.0));
        const Counts off = CountRequests(ellipsoid, eye, target, false, roots);
        const Counts on = CountRequests(ellipsoid, eye, target, true, roots);
        std::printf("%-20s %5d/%-5d        %5d/%-5d             %5d/%-5d                %5d\n",
                    scenario.name, off.requested, on.requested,
                    off.imageFiles, on.imageFiles, off.terrainFiles, on.terrainFiles,
                    on.horizonCulled);
        Check("horizon never increases requests", on.requested <= off.requested);
        Check("horizon never increases selected image files", on.imageFiles <= off.imageFiles);
        Check("horizon never increases selected terrain files", on.terrainFiles <= off.terrainFiles);
    }
    return failures ? 1 : 0;
}
