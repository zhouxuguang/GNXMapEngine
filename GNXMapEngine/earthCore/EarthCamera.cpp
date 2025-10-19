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

const static double MIN_EYE_DISTANCE = 20;

static const void CalAngles(const Geodetic3D& eyeGeodetic, const Geodetic3D& eyeGeodeticTarget, const Vector3d& eyePos, 
    double &azimuthAngle, double& verticalAngle)
{

}

Vector3d GetDirInEyeSpace(double azimuthAngle, double verticalAngle)
{
    Vector3d dir;
    dir.x = sin(verticalAngle) * sin(azimuthAngle);
    dir.y = sin(verticalAngle) * cos(azimuthAngle);
    dir.z = -cos(verticalAngle);

    return dir;
}

EarthCamera::EarthCamera(const Ellipsoid& ellipsoid, const std::string& name) :
    mEllipsoid(ellipsoid),
    Camera(GetRenderDevice()->GetRenderDeviceType(), name),
    mEyeGeodetic(degToRad(110), degToRad(23), 6398140),
    mEyeGeodeticTarget(degToRad(110), degToRad(23), 0)
{
    mEyePos = ellipsoid.CartographicToCartesian(mEyeGeodetic);

    // 计算水平和垂直视角
    CalAngles(mEyeGeodetic, mEyeGeodeticTarget, mEyePos, mAzimuthAngle, mVerticalAngle);

    // 计算水平角变换的矩阵
    Matrix4x4d azimuthMatrix = Matrix4x4d::CreateRotation(0, 1, 0, -radToDeg(mAzimuthAngle));

    mTargetPos = ellipsoid.CartographicToCartesian(mEyeGeodeticTarget);

    double eyeDistance = (mTargetPos - mEyePos).Length();

    // 获得视线在眼空间中的方向
    Vector3d viewDirInEye = GetDirInEyeSpace(mAzimuthAngle, mVerticalAngle);
    
    // 用于计算椭球空间到视空间的坐标转换，对应于博士论文中5. 3. 1. 2 椭球空间变换到眼空间
    mEllipsoidToEye = Matrix4x4d::CreateRotation(1, 0, 0, -90 + radToDeg(mEyeGeodetic.latitude)) *
                            Matrix4x4d::CreateRotation(0, 0, 1, -90 - radToDeg(mEyeGeodetic.longitude)) *
                            Matrix4x4d::CreateTranslate(-mEyePos);
    
    // 用于计算视空间到椭球空间的坐标转换，对应于博士论文中5. 3. 1. 1 眼空间变换到椭球空间
    mEyeToEllipsoid = Matrix4x4d::CreateRotation(0, 0, 1, 90 + radToDeg(mEyeGeodetic.longitude)) *
                        Matrix4x4d::CreateRotation(1, 0, 0, 90 - radToDeg(mEyeGeodetic.latitude)) *
                        Matrix4x4d::CreateTranslate(mEyePos);

    Vector3d viewDirInWorld = (mEyeToEllipsoid.GetMatrix3() * viewDirInEye).Normalize();

    Vector3d newEyePos = mTargetPos - viewDirInWorld * eyeDistance;
    mEyePos = newEyePos;
    
    // 计算局部的东北天的坐标轴向
    Matrix4x4d eastNorthUp = GeoTransform::eastNorthUpToFixedFrame(mEyePos, ellipsoid);
    Vector4d north = eastNorthUp.col(1);
    north = azimuthMatrix * north;
    
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
    if (dist + deltaDistance <= MIN_EYE_DISTANCE && deltaDistance < 0)
    {
        dist = MIN_EYE_DISTANCE;
    }
    else
    {
        dist += deltaDistance;
    }
    
    //计算视线方向
    Vector3d viewDir = (mEyePos - mTargetPos).Normalize();
    mEyePos = mTargetPos + viewDir * dist;
    
    // 计算新的视点的地理坐标
    mEyeGeodetic = mEllipsoid.CartesianToCartographic(mEyePos);
    
    // 计算局部的东北天的坐标轴向
    Matrix4x4d eastNorthUp = GeoTransform::eastNorthUpToFixedFrame(mEyePos, mEllipsoid);
    Vector4d north = eastNorthUp.col(1);

	// 计算水平角变换的矩阵
	Matrix4x4d azimuthMatrix = Matrix4x4d::CreateRotation(0, 1, 0, -radToDeg(mAzimuthAngle));
    north = azimuthMatrix * north;
    
    LookAt(Vector3f(mEyePos.x, mEyePos.y, mEyePos.z),
           Vector3f(mTargetPos.x, mTargetPos.y, mTargetPos.z),
           Vector3f(north.x, north.y, north.z));

    {
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
    
    double eyeDistance = (mTargetPos - mEyePos).Length();
    
    // 计算出新的注视点的坐标
    Geodetic3D geodeticPoint = mEllipsoid.CartesianToCartographic(intersectPoint);
    mEyeGeodeticTarget = Geodetic3D(geodeticPoint.longitude, geodeticPoint.latitude);
    mTargetPos = mEllipsoid.CartographicToCartesian(mEyeGeodeticTarget);
    
    // 获得视线在眼空间中的方向
    Vector3d viewDirInEye = GetDirInEyeSpace(mAzimuthAngle, mVerticalAngle);
    
    // 用于计算视空间到椭球空间的坐标转换，对应于博士论文中5. 3. 1. 1 眼空间变换到椭球空间
    mEyeToEllipsoid = Matrix4x4d::CreateRotation(0, 0, 1, 90 + radToDeg(mEyeGeodeticTarget.longitude)) *
                        Matrix4x4d::CreateRotation(1, 0, 0, 90 - radToDeg(mEyeGeodeticTarget.latitude)) *
                        Matrix4x4d::CreateTranslate(mEyePos);

    Vector3d viewDirInWorld = (mEyeToEllipsoid.GetMatrix3() * viewDirInEye).Normalize();

    // 计算新的视点坐标
    Vector3d newEyePos = mTargetPos - viewDirInWorld * eyeDistance;
    mEyePos = newEyePos;
    mEyeGeodetic = mEllipsoid.CartesianToCartographic(mEyePos);
    
    double lont = radToDeg(geodeticPoint.longitude);
    double lat = radToDeg(geodeticPoint.latitude);
    printf("pan point lont = %lf, lat = %lf\n", lont, lat);
    
    mEyePos = mEllipsoid.CartographicToCartesian(mEyeGeodetic);
    
    // 计算局部的东北天的坐标轴向
    Matrix4x4d eastNorthUp = GeoTransform::eastNorthUpToFixedFrame(mEyePos, mEllipsoid);
    Vector4d north = eastNorthUp.col(1);

	// 计算水平角变换的矩阵
	Matrix4x4d azimuthMatrix = Matrix4x4d::CreateRotation(0, 1, 0, -radToDeg(mAzimuthAngle));
	north = azimuthMatrix * north;
    
    LookAt(Vector3f(mEyePos.x, mEyePos.y, mEyePos.z),
           Vector3f(mTargetPos.x, mTargetPos.y, mTargetPos.z),
           Vector3f(north.x, north.y, north.z));
}

EARTH_CORE_NAMESPACE_END
