//
//  Ellipsoid.h
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#ifndef EARTH_ENGINE_ELLIPSOID_INCLUDE_H
#define EARTH_ENGINE_ELLIPSOID_INCLUDE_H

#include "EarthEngineDefine.h"
#include "Geodetic3D.h"

//大地椭球体

EARTH_CORE_NAMESPACE_BEGIN

class Ellipsoid
{
public:
    Ellipsoid(double x, double y, double z);
    
    Ellipsoid(const Vector3d &vec3);
    
    // 获得三个轴的长度
    Vector3d GetAxis() const;
    
    Vector3d GetOneOverRadii() const;
    
    //获得三个轴的平方倒数
    Vector3d GetOneOverRadiiSquared() const;
    
    //计算椭球上一点某一点处的法向量
    Vector3d GeodeticSurfaceNormal(const Vector3d& positionOnEllipsoid) const;
    
    //根据经纬度和大地高计算椭球体上的法线向量
    Vector3d GeodeticSurfaceNormal(const Geodetic3D& geodetic) const;
    
    // 地理坐标转笛卡尔坐标
    Vector3d CartographicToCartesian(const Geodetic3D& cartographic) const;
    
    // 笛卡尔坐标转地理坐标
    Geodetic3D CartesianToCartographic(const Vector3d& cartesian) const;
    
    // 缩放到大地法线
    Vector3d ScaleToGeodeticSurface(const Vector3d& cartesian) const;
    
private:
    Vector3d mAxisLength;  //三轴的长度
    //Vector3d mRadii;
    Vector3d mRadiiSquared;
    Vector3d mOneOverRadii;
    Vector3d mOneOverRadiiSquared;   // 1/a2
    
public:
    // https://earth-info.nga.mil/GandG/publications/tr8350.2/wgs84fin.pdf
    static const Ellipsoid WGS84;
    static const Ellipsoid UnitSphere;
};

EARTH_CORE_NAMESPACE_END

#endif /* EARTH_ENGINE_ELLIPSOID_INCLUDE_H */
