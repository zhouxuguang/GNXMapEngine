//
//  EarthCameraPose.cpp
//  GNXMapEngine
//
//  方位角 / 俯仰角姿态数学的实现。全部为解析式，无迭代、无状态。
//

#include "EarthCameraPose.h"

#include <algorithm>
#include <cmath>

EARTH_CORE_NAMESPACE_BEGIN

namespace
{
    const double kTwoPi = 6.283185307179586476925286766559;
    const double kHalfPi = 1.570796326794896619231321691640;

    // 视线水平分量退化阈值：归一化后的水平分量长度即 sin(pitch)。
    // 小于该值意味着俯仰角几乎为 0（垂直下视），此时方位角在数学上无定义。
    const double kHorizontalDegenerateEpsilon = 1e-9;

    // up 投影退化阈值：投影结果长度平方即 sin²(pitch)。
    // 小于该值时认为视线与大地法线平行（垂直下视），改用方位角约定值。
    const double kUpProjectionDegenerateEpsilonSq = 1e-12;

    // 方位角吸附阈值：与 2π 的差小于该值即视为 0（约 5.7e-8 度，不会掩盖任何真实角度）
    const double kAzimuthSnapEpsilon = 1e-9;

    // 拖拽换算用的 FOV 合法区间（度），越界时退回默认视场，避免「拖拽毫无反应」
    const double kMinFovDegrees = 1.0;
    const double kMaxFovDegrees = 179.0;
    const double kDefaultFovDegrees = 60.0;

    // 灵敏度兜底：非有限值按 0（该维度不响应），负值保留以便反向试手感
    double SanitizeSensitivity(double sensitivity)
    {
        return std::isfinite(sensitivity) ? sensitivity : 0.0;
    }
}

Vector3d EunFrame::ToWorld(const Vector3d& vectorInEun) const
{
    return vectorInEun.x * east + vectorInEun.y * north + vectorInEun.z * up;
}

Vector3d EunFrame::ToEun(const Vector3d& vectorInWorld) const
{
    return Vector3d(vectorInWorld.DotProduct(east),
                    vectorInWorld.DotProduct(north),
                    vectorInWorld.DotProduct(up));
}

EunFrame EarthCameraPose::BuildEunFrame(const Vector3d& origin, const Ellipsoid& ellipsoid)
{
    // 不能直接用 GeoTransform::eastNorthUpToFixedFrame / Ellipsoid::GeodeticSurfaceNormal
    // 来取 up：那两个实现隐含「点位于椭球面上」的前提（up ∝ origin / radii²），
    // 对离面点（例如 200km 高空的视点）会引入量级约 0.04 度的法线方向误差，
    // 直接体现为「视点处量测」的俯仰角/方位角偏差。
    // 这里先投影到椭球面得到严格的大地经纬度，再按定义构造大地法线。
    const Geodetic3D geodetic = ellipsoid.CartesianToCartographic(origin);
    const Vector3d up = ellipsoid.GeodeticSurfaceNormal(geodetic);

    // 东向：与地轴垂直、且方位角等于该点经度（x、y 半径相同，故与 xy 方位一致）
    Vector3d east = Vector3d(-origin.y, origin.x, 0.0);
    if (east.LengthSq() < Epsilon14)
    {
        // 极点附近退化
        east = Vector3d(1.0, 0.0, 0.0);
    }
    east.Normalize();

    // 北向 = 天 × 东（右手系；与 GeoTransform 的约定一致）
    const Vector3d north = Vector3d::CrossProduct(up, east);

    EunFrame frame;
    frame.east = east;
    frame.north = north;
    frame.up = up;
    return frame;
}

Vector3d EarthCameraPose::ViewDirectionFromAngles(double azimuthRad, double pitchRad)
{
    azimuthRad = NormalizeAzimuth(azimuthRad);
    pitchRad = ClampPitch(pitchRad);
    const double sinPitch = sin(pitchRad);
    return Vector3d(sinPitch * sin(azimuthRad),
                    sinPitch * cos(azimuthRad),
                    -cos(pitchRad));
}

Vector3d EarthCameraPose::EyeFromTargetAndAngles(const Vector3d& targetPos,
                                                const Ellipsoid& ellipsoid,
                                                double azimuthRad,
                                                double pitchRad,
                                                double distance)
{
    const EunFrame frame = BuildEunFrame(targetPos, ellipsoid);

    // 直接复用公开的视线方向公式，避免“测试的公式”和“生产的公式”
    // 成为两份会分叉的实现。视点位于视线方向的反向。
    const Vector3d targetToEye = -frame.ToWorld(ViewDirectionFromAngles(azimuthRad, pitchRad));

    return targetPos + distance * targetToEye;
}

Vector3d EarthCameraPose::LookUpForLookAt(const Vector3d& eyePos,
                                          const Vector3d& targetPos,
                                          const Ellipsoid& ellipsoid,
                                          double azimuthRad)
{
    const EunFrame frame = BuildEunFrame(eyePos, ellipsoid);

    Vector3d forward = targetPos - eyePos;
    const double forwardLength = forward.Length();
    if (forwardLength > Epsilon14)
    {
        forward /= forwardLength;
    }

    // 视点处大地法线在「与视线垂直的平面」内的投影。
    // 由构造可知结果与 forward 精确正交，因此相机右向量 = cross(forward, up) 垂直于
    // 视点处大地法线，即落在当地水平面内 —— 地平线必然水平，不存在滚转。
    Vector3d up = frame.up - forward.DotProduct(frame.up) * forward;

    if (up.LengthSq() < kUpProjectionDegenerateEpsilonSq)
    {
        // 垂直下视：法线与视线平行，投影退化为零向量。
        // 约定以「方位角对应的水平方向」为画面向上（方位角 0 时即正北朝上），
        // 该值与俯仰角 → 0 时的投影极限一致，因此不会出现跳变。
        up = cos(azimuthRad) * frame.north + sin(azimuthRad) * frame.east;

        if (up.LengthSq() < kUpProjectionDegenerateEpsilonSq)
        {
            up = frame.north;
        }
    }

    up.Normalize();
    return up;
}

void EarthCameraPose::AnglesFromEyeTarget(const Vector3d& eyePos,
                                          const Vector3d& targetPos,
                                          const Ellipsoid& ellipsoid,
                                          bool measureAtEye,
                                          double& outAzimuthRad,
                                          double& outPitchRad)
{
    const Vector3d measureOrigin = measureAtEye ? eyePos : targetPos;
    const EunFrame frame = BuildEunFrame(measureOrigin, ellipsoid);

    // 俯仰角：视线方向的反方向（目标点 -> 视点）与量测点大地法线的夹角。
    // 这里用 atan2(水平分量长度, 竖直分量) 而不是 acos(点积)：acos 在 0 度与 90 度
    // 附近导数发散，会把 ~1e-16 的浮点误差放大成 ~1e-6 度的误差（实测 1.2e-6 度），
    // 而 atan2 在两端都保持稳定。
    Vector3d targetToEye = eyePos - targetPos;
    const double targetToEyeLength = targetToEye.Length();
    if (targetToEyeLength > Epsilon14)
    {
        targetToEye /= targetToEyeLength;
    }

    const double verticalComponent = targetToEye.DotProduct(frame.up);
    const Vector3d horizontalComponent = targetToEye - verticalComponent * frame.up;
    const double horizontalComponentLength = horizontalComponent.Length();

    outPitchRad = ClampPitch(atan2(horizontalComponentLength, verticalComponent));

    // 方位角：视线方向（视点 -> 目标点）在水平面内的投影与正北方向的夹角
    const Vector3d viewDir = targetPos - eyePos;
    const double eastComponent = viewDir.DotProduct(frame.east);
    const double northComponent = viewDir.DotProduct(frame.north);

    if (sqrt(eastComponent * eastComponent + northComponent * northComponent) < kHorizontalDegenerateEpsilon)
    {
        // 垂直下视：水平投影退化为零，方位角无定义，约定为 0
        outAzimuthRad = 0.0;
    }
    else
    {
        outAzimuthRad = NormalizeAzimuth(atan2(eastComponent, northComponent));
    }
}

double EarthCameraPose::NormalizeAzimuth(double azimuthRad)
{
    if (!std::isfinite(azimuthRad))
    {
        return 0.0;
    }

    double result = fmod(azimuthRad, kTwoPi);
    if (result < 0.0)
    {
        result += kTwoPi;
    }

    // 方位角 0 与 360 在几何上是同一个方向。反算方位角时水平分量只会有 ~1e-19 的
    // 浮点残差（符号还可能是负的），直接规约会得到 359.9999999999 这种读数，
    // 在面板上显示成 360.000 反而像是越界。这里把极靠近 2π 的结果吸附回 0。
    if (kTwoPi - result < kAzimuthSnapEpsilon)
    {
        result = 0.0;
    }
    return result;
}

double EarthCameraPose::ClampPitch(double pitchRad)
{
    if (!std::isfinite(pitchRad))
    {
        return 0.0;
    }
    return Clamp(pitchRad, 0.0, kHalfPi);
}

double EarthCameraPose::AnglePerPixelRadians(double fovYDegrees, double viewportHeightPixels)
{
    if (!std::isfinite(viewportHeightPixels) || viewportHeightPixels <= 0.0)
    {
        // 视口高度未知（如首帧 Resize 之前）：无法把像素换算成角度
        return 0.0;
    }

    double fov = fovYDegrees;
    if (!std::isfinite(fov) || fov < kMinFovDegrees || fov > kMaxFovDegrees)
    {
        fov = kDefaultFovDegrees;
    }

    const double halfFovRad = ToRadians(fov) * 0.5;
    const double tanHalfFov = tan(halfFovRad);
    if (!std::isfinite(tanHalfFov) || tanHalfFov <= 0.0)
    {
        return 0.0;
    }

    return 2.0 * tanHalfFov / viewportHeightPixels;
}

CameraDragResult EarthCameraPose::ApplyDragToPose(double azimuthRad,
                                                  double pitchRad,
                                                  double distance,
                                                  double dxPixels,
                                                  double dyPixels,
                                                  CameraDragMode mode,
                                                  double fovYDegrees,
                                                  double viewportHeightPixels,
                                                  double minDistance,
                                                  double azimuthSensitivity,
                                                  double pitchSensitivity,
                                                  double zoomSensitivity)
{
    // 先规约输入姿态：即使增量为 0 或非法，返回值也一定是合法姿态
    const double minDist = (std::isfinite(minDistance) && minDistance > 0.0) ? minDistance : 0.0;
    CameraDragResult result;
    result.azimuthRad = NormalizeAzimuth(azimuthRad);
    result.pitchRad = ClampPitch(pitchRad);
    result.distance = std::max(std::isfinite(distance) ? distance : 0.0, minDist);

    if (!std::isfinite(dxPixels) || !std::isfinite(dyPixels))
    {
        return result;
    }

    const double anglePerPixel = AnglePerPixelRadians(fovYDegrees, viewportHeightPixels);

    if (mode == CameraDragMode::RightButton)
    {
        // 向右拖 -> 方位角增大（画面内容逆时针）
        result.azimuthRad = NormalizeAzimuth(result.azimuthRad +
            dxPixels * anglePerPixel * SanitizeSensitivity(azimuthSensitivity));

        // 向下拖等比放大：每拖满半个视口高度恰好放大/缩小一倍
        const double halfViewportHeight = viewportHeightPixels * 0.5;
        if (std::isfinite(halfViewportHeight) && halfViewportHeight > 0.0)
        {
            const double scale = pow(2.0, -dyPixels * SanitizeSensitivity(zoomSensitivity) / halfViewportHeight);
            const double scaledDistance = result.distance * scale;
            if (std::isfinite(scaledDistance))
            {
                result.distance = std::max(scaledDistance, minDist);
            }
        }
    }
    else
    {
        // 中键向下拖 -> 俯仰角增大（向地平线倾斜）；横向忽略
        result.pitchRad = ClampPitch(result.pitchRad +
            dyPixels * anglePerPixel * SanitizeSensitivity(pitchSensitivity));
    }

    return result;
}

EARTH_CORE_NAMESPACE_END
