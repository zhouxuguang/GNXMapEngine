//
//  EarthCamera.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/7/2.
//

#include "EarthCamera.h"
#include "EarthCameraPose.h"
#include "IntersectionTests.h"
#include "Runtime/BaseLib/include/LogService.h"

#include <algorithm>
#include <cmath>

EARTH_CORE_NAMESPACE_BEGIN

namespace
{
    // 视点与目标点的最小距离，避免相机穿到地下
    const double MIN_EYE_DISTANCE = 20.0;
}

EarthCamera::EarthCamera(const Ellipsoid& ellipsoid, const std::string& name) :
    mEllipsoid(ellipsoid),
    Camera(GetRenderDevice()->GetRenderDeviceType(), name),
    mEyeGeodetic(EarthCameraPose::ToRadians(110.0), EarthCameraPose::ToRadians(23.0), 6398140),
    mEyeGeodeticTarget(EarthCameraPose::ToRadians(110.0), EarthCameraPose::ToRadians(23.0), 0)
{
    mTargetPos = mEllipsoid.CartographicToCartesian(mEyeGeodeticTarget);
    mEyePos = mEllipsoid.CartographicToCartesian(mEyeGeodetic);

    mEyeDistance = std::max((mEyePos - mTargetPos).Length(), MIN_EYE_DISTANCE);

    // 用「目标点处量测」反算初始角度：ApplyPose() 以目标点为基准重建视点，
    // 这样初始给定的视点位置能被精确复现，而不会在构造阶段漂移。
    EarthCameraPose::AnglesFromEyeTarget(mEyePos, mTargetPos, mEllipsoid, false, mAzimuthAngle, mPitchAngle);

    ApplyPose();

    LOG_INFO("EarthCamera initialized: eye(lon=%.6f, lat=%.6f, h=%.1f) target(lon=%.6f, lat=%.6f, h=%.1f) "
             "azimuth=%.4f deg pitch=%.4f deg distance=%.1f m",
             EarthCameraPose::ToDegrees(mEyeGeodetic.longitude),
             EarthCameraPose::ToDegrees(mEyeGeodetic.latitude),
             mEyeGeodetic.height,
             EarthCameraPose::ToDegrees(mEyeGeodeticTarget.longitude),
             EarthCameraPose::ToDegrees(mEyeGeodeticTarget.latitude),
             mEyeGeodeticTarget.height,
             EarthCameraPose::ToDegrees(mAzimuthAngle),
             EarthCameraPose::ToDegrees(mPitchAngle),
             mEyeDistance);
}

EarthCamera::~EarthCamera() = default;

void EarthCamera::SetAzimuthAngle(double azimuthRad)
{
    if (!std::isfinite(azimuthRad))
    {
        LOG_ERROR("EarthCamera::SetAzimuthAngle ignored non-finite value");
        return;
    }
    mAzimuthAngle = EarthCameraPose::NormalizeAzimuth(azimuthRad);
    ApplyPose();
}

double EarthCamera::GetAzimuthAngle() const
{
    double azimuthRad = 0.0;
    double pitchRad = 0.0;
    EarthCameraPose::AnglesFromEyeTarget(mEyePos, mTargetPos, mEllipsoid, true, azimuthRad, pitchRad);
    return azimuthRad;
}

double EarthCamera::GetAzimuthAngleAtTarget() const
{
    // 返回命令状态而非重新反算。垂直下视时水平投影为零，
    // 反算只能得到“无定义”；但方位状态仍决定画面朝上方向和后续倾斜方向。
    return mAzimuthAngle;
}

double EarthCamera::GetAzimuthAngleDegrees() const
{
    return EarthCameraPose::ToDegrees(GetAzimuthAngle());
}

double EarthCamera::GetAzimuthAngleAtTargetDegrees() const
{
    return EarthCameraPose::ToDegrees(GetAzimuthAngleAtTarget());
}

void EarthCamera::SetPitchAngle(double pitchRad)
{
    if (!std::isfinite(pitchRad))
    {
        LOG_ERROR("EarthCamera::SetPitchAngle ignored non-finite value");
        return;
    }
    mPitchAngle = EarthCameraPose::ClampPitch(pitchRad);
    ApplyPose();
}

double EarthCamera::GetPitchAngle() const
{
    double azimuthRad = 0.0;
    double pitchRad = 0.0;
    EarthCameraPose::AnglesFromEyeTarget(mEyePos, mTargetPos, mEllipsoid, true, azimuthRad, pitchRad);
    return pitchRad;
}

double EarthCamera::GetPitchAngleAtTarget() const
{
    return mPitchAngle;
}

double EarthCamera::GetPitchAngleDegrees() const
{
    return EarthCameraPose::ToDegrees(GetPitchAngle());
}

double EarthCamera::GetPitchAngleAtTargetDegrees() const
{
    return EarthCameraPose::ToDegrees(GetPitchAngleAtTarget());
}

void EarthCamera::SetAzimuthPitch(double azimuthRad, double pitchRad)
{
    if (!std::isfinite(azimuthRad) || !std::isfinite(pitchRad))
    {
        LOG_ERROR("EarthCamera::SetAzimuthPitch ignored non-finite value(s)");
        return;
    }
    mAzimuthAngle = EarthCameraPose::NormalizeAzimuth(azimuthRad);
    mPitchAngle = EarthCameraPose::ClampPitch(pitchRad);
    ApplyPose();
}

void EarthCamera::SetAzimuthPitchDegrees(double azimuthDegrees, double pitchDegrees)
{
    SetAzimuthPitch(EarthCameraPose::ToRadians(azimuthDegrees), EarthCameraPose::ToRadians(pitchDegrees));
}

void EarthCamera::SetEyeDistance(double distance)
{
    if (!std::isfinite(distance))
    {
        LOG_ERROR("EarthCamera::SetEyeDistance ignored non-finite value");
        return;
    }
    mEyeDistance = distance;
    ApplyPose();
}

void EarthCamera::SetEyeGeodetic(const Geodetic3D& eyeGeodetic)
{
    if (!std::isfinite(eyeGeodetic.longitude) || !std::isfinite(eyeGeodetic.latitude) ||
        !std::isfinite(eyeGeodetic.height))
    {
        LOG_ERROR("EarthCamera::SetEyeGeodetic ignored non-finite coordinate");
        return;
    }
    mEyeGeodetic = eyeGeodetic;
    mEyePos = mEllipsoid.CartographicToCartesian(mEyeGeodetic);

    mEyeDistance = std::max((mEyePos - mTargetPos).Length(), MIN_EYE_DISTANCE);

    // 目标点不变，按新视点反算角度（目标点基准，保证与位置自洽）
    EarthCameraPose::AnglesFromEyeTarget(mEyePos, mTargetPos, mEllipsoid, false, mAzimuthAngle, mPitchAngle);

    ApplyPose();
}

void EarthCamera::SetEyeGeodeticTarget(const Geodetic3D& targetGeodetic)
{
    if (!std::isfinite(targetGeodetic.longitude) || !std::isfinite(targetGeodetic.latitude) ||
        !std::isfinite(targetGeodetic.height))
    {
        LOG_ERROR("EarthCamera::SetEyeGeodeticTarget ignored non-finite coordinate");
        return;
    }
    mEyeGeodeticTarget = targetGeodetic;
    mTargetPos = mEllipsoid.CartographicToCartesian(mEyeGeodeticTarget);

    // 目标点变化后，视点按原有角度与距离跟随，保持「绕目标点观察」的语义
    ApplyPose();
}

Vector3f EarthCamera::GetViewDirection() const
{
    Vector3d direction = mTargetPos - mEyePos;
    const double length = direction.Length();
    if (length > Epsilon14)
    {
        direction /= length;
    }

    return Vector3f(direction.x, direction.y, direction.z);
}

Vector3d EarthCamera::GetViewDirectionInEyeEun() const
{
    const EunFrame frame = EarthCameraPose::BuildEunFrame(mEyePos, mEllipsoid);

    Vector3d direction = mTargetPos - mEyePos;
    const double length = direction.Length();
    if (length > Epsilon14)
    {
        direction /= length;
    }

    return frame.ToEun(direction);
}

void EarthCamera::Zoom(double deltaDistance)
{
    SetEyeDistance(mEyeDistance + deltaDistance);
}

void EarthCamera::Pan(float offsetX, float offsetY)
{
    Vector3d newTarget;
    double newAzimuth = mAzimuthAngle;
    if (!EarthCameraPose::PanOnEllipsoid(mEyePos, mTargetPos, mEllipsoid, mAzimuthAngle,
                                         offsetX, offsetY, GetFOV(), mWidth, mHeight,
                                         newTarget, newAzimuth))
    {
        return;
    }

    mEyeGeodeticTarget = mEllipsoid.CartesianToCartographic(newTarget);
    mTargetPos = mEllipsoid.CartographicToCartesian(mEyeGeodeticTarget);
    mAzimuthAngle = newAzimuth;
    ApplyPose();
}

void EarthCamera::OrbitByDrag(double dxPixels, double dyPixels, CameraDragMode mode)
{
    if (!std::isfinite(dxPixels) || !std::isfinite(dyPixels))
    {
        LOG_ERROR("EarthCamera::OrbitByDrag ignored non-finite pixel delta (dx=%f dy=%f)", dxPixels, dyPixels);
        return;
    }

    // 手感只取决于相机自身的 FOV 与视口高度（见 EarthCameraPose）
    const Vector2i viewSize = GetViewSize();
    const CameraDragResult drag = EarthCameraPose::ApplyDragToPose(mAzimuthAngle,
                                                                  mPitchAngle,
                                                                  mEyeDistance,
                                                                  dxPixels,
                                                                  dyPixels,
                                                                  mode,
                                                                  static_cast<double>(GetFOV()),
                                                                  static_cast<double>(viewSize.y),
                                                                  MIN_EYE_DISTANCE,
                                                                  mAzimuthDragSensitivity,
                                                                  mPitchDragSensitivity,
                                                                  mZoomDragSensitivity);

    // 只写回角度与距离（目标点不变），由 ApplyPose 统一重算视点与视图矩阵
    mAzimuthAngle = drag.azimuthRad;
    mPitchAngle = drag.pitchRad;
    mEyeDistance = drag.distance;
    ApplyPose();
}

void EarthCamera::SetDragSensitivity(double azimuthSensitivity, double pitchSensitivity, double zoomSensitivity)
{
    mAzimuthDragSensitivity = std::isfinite(azimuthSensitivity) ? azimuthSensitivity : 0.0;
    mPitchDragSensitivity = std::isfinite(pitchSensitivity) ? pitchSensitivity : 0.0;
    mZoomDragSensitivity = std::isfinite(zoomSensitivity) ? zoomSensitivity : 0.0;
}

void EarthCamera::ApplyPose()
{
    // 角度与距离统一钳制到合法区间
    mAzimuthAngle = EarthCameraPose::NormalizeAzimuth(mAzimuthAngle);
    mPitchAngle = EarthCameraPose::ClampPitch(mPitchAngle);
    mEyeDistance = std::max(mEyeDistance, MIN_EYE_DISTANCE);

    // 由「目标点 + 方位角 + 俯仰角 + 距离」解析求出视点，恒满足 |eye - target| == 距离
    mEyePos = EarthCameraPose::EyeFromTargetAndAngles(mTargetPos, mEllipsoid,
                                                     mAzimuthAngle, mPitchAngle, mEyeDistance);
    mEyeGeodetic = mEllipsoid.CartesianToCartographic(mEyePos);

    // up 取「视点处大地法线在视线垂直面内的投影」，保证地平线水平、画面无滚转；
    // 垂直下视时退化为按方位角确定的水平方向（正北朝上）。
    const Vector3d up = EarthCameraPose::LookUpForLookAt(mEyePos, mTargetPos, mEllipsoid, mAzimuthAngle);

    // ECEF 坐标量级约 6.4e6 m，float 在该量级的间距约为 0.5 m。
    // 若先把 eye/target 转成 float 再让 Camera::LookAt 相减，20 m 近地视角
    // 会在归一化前已经丢失明显的方向精度。先以 double 完成视图矩阵，
    // 最后才逐元素降精度供现有渲染管线使用。
    const Matrix4x4d preciseView = Matrix4x4d::CreateLookAt(mEyePos, mTargetPos, up);
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            mView[row][column] = static_cast<float>(preciseView[row][column]);
        }
    }

    // 保持 Camera 基类的公开状态与视图矩阵一致；GenerateRay/渲染仍按原接口工作。
    mPosition = Vector3f(mEyePos.x, mEyePos.y, mEyePos.z);
    mLook = Vector3f(mTargetPos.x, mTargetPos.y, mTargetPos.z);
    mUp = Vector3f(up.x, up.y, up.z);
    mViewDirty = false;
}

EARTH_CORE_NAMESPACE_END
