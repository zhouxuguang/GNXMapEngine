//
//  EarthCamera.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/7/2.
//

#include "EarthCamera.h"
#include "IntersectionTests.h"
#include "GeoTransform.h"

EARTH_CORE_NAMESPACE_BEGIN

EarthCamera::EarthCamera(const Ellipsoid& ellipsoid, const std::string& name) :
    mEllipsoid(ellipsoid),
    Camera(getRenderDevice()->getRenderDeviceType(), name),
    mEyeGeodetic(degToRad(110), degToRad(23), 6398140),
    mEyeGeodeticTarget(degToRad(110), degToRad(23), 0)
{
    mEyePos = ellipsoid.CartographicToCartesian(mEyeGeodetic);
    mTargetPos = ellipsoid.CartographicToCartesian(mEyeGeodeticTarget);
    
    // 用于计算椭球空间到视空间的坐标转换，对应于博士论文中5. 3. 1. 2 椭球空间变换到眼空间
    mEllipsoidToEye = Matrix4x4d::CreateRotation(1, 0, 0, -90 + radToDeg(mEyeGeodetic.latitude)) *
                            Matrix4x4d::CreateRotation(0, 0, 1, -90 - radToDeg(mEyeGeodetic.longitude)) *
                            Matrix4x4d::CreateTranslate(-mEyePos);
    
    // 用于计算视空间到椭球空间的坐标转换，对应于博士论文中5. 3. 1. 1 眼空间变换到椭球空间
    mEyeToEllipsoid = Matrix4x4d::CreateRotation(0, 0, 1, 90 + radToDeg(mEyeGeodetic.longitude)) *
                        Matrix4x4d::CreateRotation(1, 0, 0, 90 - radToDeg(mEyeGeodetic.latitude)) *
                        Matrix4x4d::CreateTranslate(mEyePos);
    
    Vector3f zAxis = Vector3f(mEyeToEllipsoid[0][1], mEyeToEllipsoid[1][1], mEyeToEllipsoid[2][1]);
    
    // 计算局部的东北天的坐标轴向
    Matrix4x4d eastNorthUp = GeoTransform::eastNorthUpToFixedFrame(mEyePos, ellipsoid);
    Vector4d north = eastNorthUp.col(1);
    
    LookAt(Vector3f(mEyePos.x, mEyePos.y, mEyePos.z),
           Vector3f(mTargetPos.x, mTargetPos.y, mTargetPos.z),
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

void EarthCamera::Zoom(double deltaDistance)
{
    double dist = (mEyePos - mTargetPos).Length();
    printf("view point lont = %lf, lat = %lf, height = %lf, dsit = %lf, deltaDistance = %lf\n",
           radToDeg(mEyeGeodetic.longitude), radToDeg(mEyeGeodetic.latitude), mEyeGeodetic.height, dist, deltaDistance);
    
    // 当前距离加上增量小于最小距离，那么就停止缩放了
    if (dist + deltaDistance <= 10 && deltaDistance < 0)
    {
        return;
    }
    
    dist += deltaDistance;
    //计算视线方向
    Vector3d viewDir = (mEyePos - mTargetPos).Normalize();
    mEyePos = mTargetPos + viewDir * dist;
    
    // 计算新的视点的地理坐标
    mEyeGeodetic = mEllipsoid.CartesianToCartographic(mEyePos);
    
    // 计算局部的东北天的坐标轴向
    Matrix4x4d eastNorthUp = GeoTransform::eastNorthUpToFixedFrame(mEyePos, mEllipsoid);
    Vector4d north = eastNorthUp.col(1);
    
    LookAt(Vector3f(mEyePos.x, mEyePos.y, mEyePos.z),
           Vector3f(mTargetPos.x, mTargetPos.y, mTargetPos.z),
           Vector3f(north.x, north.y, north.z));
    
    // for test
    Vector3d origin = Vector3d(mEyePos.x, mEyePos.y, mEyePos.z);
    Vector3d direction = Vector3d(-viewDir.x, -viewDir.y, -viewDir.z);
    Rayd ray(mEyePos, direction);
    
    Vector3d intersectPoint;
    bool isIntersect = IntersectionTests::RayEllipsoid(ray, mEllipsoid, intersectPoint);
    
    Geodetic3D geodeticPoint = mEllipsoid.CartesianToCartographic(intersectPoint);
    
    double lont = radToDeg(geodeticPoint.longitude);
    double lat = radToDeg(geodeticPoint.latitude);
    printf("inter point lont = %lf, lat = %lf\n", lont, lat);
    
    /*Ray ray1 = GenerateRay(800, 400);
    
    isIntersect = IntersectionTests::RayEllipsoid(ray1, mEllipsoid, intersectPoint);*/
    
    geodeticPoint = mEllipsoid.CartesianToCartographic(intersectPoint);
    
    lont = radToDeg(geodeticPoint.longitude);
    lat = radToDeg(geodeticPoint.latitude);
    printf("zoom inter point lont = %lf, lat = %lf\n", lont, lat);
}

void EarthCamera::Pan(float offsetX, float offsetY)
{
    float centerX = mWidth / 2.0;
    float centerY = mHeight / 2.0;
    
    // 添加偏移后的屏幕坐标
    Rayf ray = GenerateRay(centerX + offsetX, centerY + offsetY);
    Vector3f origin = ray.GetOrigin();
    Vector3f direction = ray.GetDirection();
    Rayd ray1 = Rayd(Vector3d(origin.x, origin.y, origin.z), Vector3d(direction.x, direction.y, direction.z));
    
    // 计算当前鼠标点的空间直角坐标
    Vector3d intersectPoint;
    bool isIntersect = IntersectionTests::RayEllipsoid(ray1, mEllipsoid, intersectPoint);
    if (!isIntersect)
    {
        return;
    }
    
    // 计算出新的视点坐标和注视点的坐标
    Geodetic3D geodeticPoint = mEllipsoid.CartesianToCartographic(intersectPoint);
    mEyeGeodeticTarget = Geodetic3D(geodeticPoint.longitude, geodeticPoint.latitude);
    mEyeGeodetic = Geodetic3D(geodeticPoint.longitude, geodeticPoint.latitude, mEyeGeodetic.height);
    
    double lont = radToDeg(geodeticPoint.longitude);
    double lat = radToDeg(geodeticPoint.latitude);
    printf("pan point lont = %lf, lat = %lf\n", lont, lat);
    
    mEyePos = mEllipsoid.CartographicToCartesian(mEyeGeodetic);
    mTargetPos = mEllipsoid.CartographicToCartesian(mEyeGeodeticTarget);
    
    // 计算局部的东北天的坐标轴向
    Matrix4x4d eastNorthUp = GeoTransform::eastNorthUpToFixedFrame(mEyePos, mEllipsoid);
    Vector4d north = eastNorthUp.col(1);
    
    LookAt(Vector3f(mEyePos.x, mEyePos.y, mEyePos.z),
           Vector3f(mTargetPos.x, mTargetPos.y, mTargetPos.z),
           Vector3f(north.x, north.y, north.z));
}

EARTH_CORE_NAMESPACE_END
