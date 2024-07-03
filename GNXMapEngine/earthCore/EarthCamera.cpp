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
    mEyeGeodetic(degToRad(110), degToRad(23), 6398140),
    mEyeGeodeticCenter(degToRad(110), degToRad(23), 0)
{
    Vector3d eyePos = ellipsoid.CartographicToCartesian(mEyeGeodetic);
    Vector3d targetPos = ellipsoid.CartographicToCartesian(mEyeGeodeticCenter);
    
    // 计算视坐标系的坐标轴的朝向
    double latitude = radToDeg(mEyeGeodetic.latitude);
    double longitude = radToDeg(mEyeGeodetic.longitude);
//    Matrix4x4d viewMatrix = Matrix4x4d::CreateRotation(1, 0, 0, 90 - radToDeg(mEyeGeodetic.latitude)) *
//                            Matrix4x4d::CreateRotation(0, 0, 1, 90 + radToDeg(mEyeGeodetic.longitude));
    Matrix4x4d viewMatrix = Matrix4x4d::CreateRotation(0, 0, 1, -90 - radToDeg(mEyeGeodetic.longitude)) *
                        Matrix4x4d::CreateRotation(1, 0, 0, -90 + radToDeg(mEyeGeodetic.latitude));
    
    Vector3f zAxis = Vector3f(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
    
    // 计算局部的东北天的坐标轴向
    Vector3d up = ellipsoid.GeodeticSurfaceNormal(mEyeGeodetic);
    Vector3d east = Vector3d(-eyePos.y, eyePos.x, 0.0).Normalize();
    Vector3d north = Vector3d::CrossProduct(up, east);
    
    LookAt(Vector3f(eyePos.x, eyePos.y, eyePos.z),
           Vector3f(targetPos.x, targetPos.y, targetPos.z),
           Vector3f(north.x, north.y, north.z));
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
