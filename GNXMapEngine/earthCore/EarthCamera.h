//
//  EarthCamera.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/7/2.
//

#ifndef GNX_EARTHENGINE_CORE_EARTHCAMERA_INCLUDE_JJHSGDFGFB
#define GNX_EARTHENGINE_CORE_EARTHCAMERA_INCLUDE_JJHSGDFGFB

#include "EarthEngineDefine.h"
#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

// 三维地球的相机类
class EarthCamera : public Camera
{
public:
    EarthCamera(const Ellipsoid& ellipsoid, const std::string& name);
    
    ~EarthCamera();
    
    void SetEyeGeodetic(const Geodetic3D& eyeGeodetic)
    {
        mEyeGeodetic = eyeGeodetic;
    }
    
    void SetEyeGeodeticCenter(const Geodetic3D& eyeGeodeticCenter)
    {
        mEyeGeodeticCenter = eyeGeodeticCenter;
    }
    
//    //获得相机的世界坐标位置
//    virtual Vector3f GetPosition() const;
//    
//    //获得视图矩阵
//    virtual Matrix4x4f GetViewMatrix() const;
    
    // 缩放地球
    void Zoom(double deltaDistance);
    
private:
    Ellipsoid mEllipsoid;  // 椭球体
    Geodetic3D mEyeGeodetic;  //视点的大地坐标
    Geodetic3D mEyeGeodeticCenter;   // 视坐标系的注视点
    
    Vector3d mEyePos;    //视点的空间直角坐标
    Vector3d mTargetPos;  // 注释点的空间直角坐标
    
    Matrix4x4d mEllipsoidToEye;  // 椭球空间到视空间的变换矩阵
    Matrix4x4d mEyeToEllipsoid;  // 视空间到椭球空间的变换矩阵
};

using EarthCameraPtr = std::shared_ptr<EarthCamera>;

EARTH_CORE_NAMESPACE_END

#endif /* GNX_EARTHENGINE_CORE_EARTHCAMERA_INCLUDE_JJHSGDFGFB */
