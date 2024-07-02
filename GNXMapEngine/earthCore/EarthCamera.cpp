//
//  EarthCamera.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/7/2.
//

#include "EarthCamera.h"

EARTH_CORE_NAMESPACE_BEGIN

EarthCamera::EarthCamera(const Ellipsoid& ellipsoid, const std::string& name) :
    mEllipsoid(ellipsoid),
    Camera(getRenderDevice()->getRenderDeviceType(), name),
    mEyeGeodetic(degToRad(120), degToRad(23), 6378255),
    mEyeGeodeticCenter(degToRad(120), degToRad(23), 0)
{
    Vector3d eyePos = ellipsoid.CartographicToCartesian(mEyeGeodetic);
    Vector3d targetPos = ellipsoid.CartographicToCartesian(mEyeGeodeticCenter);
    LookAt(Vector3f(eyePos.x, eyePos.y, eyePos.z),
           Vector3f(targetPos.x, targetPos.y, targetPos.z),
           Vector3f::UNIT_Z);
}

EarthCamera::~EarthCamera()
{
}

////获得相机的世界坐标位置
//Vector3f EarthCamera::GetPosition() const
//{
//    return mPosition;
//}
//
////获得视图矩阵
//Matrix4x4f EarthCamera::GetViewMatrix() const
//{
//    return mView;
//}

EARTH_CORE_NAMESPACE_END
