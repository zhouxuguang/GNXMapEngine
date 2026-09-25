//
//  Geodetic3D.h
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#ifndef GNX_MAP_ENGINE_EARTH_ENGINE_CORE_FEODETIC3D_INCLUDE_HJFJ
#define GNX_MAP_ENGINE_EARTH_ENGINE_CORE_FEODETIC3D_INCLUDE_HJFJ

#include "EarthEngineDefine.h"

EARTH_CORE_NAMESPACE_BEGIN


// 弧度表示的地理坐标，经纬度
class Geodetic3D
{
public:
    Geodetic3D(double longitudeRadians,
               double latitudeRadians,
               double heightMeters = 0.0)
    {
        this->longitude = longitudeRadians;
        this->latitude = latitudeRadians;
        this->height = heightMeters;
    }
    
    /**
      从地理坐标转换
    */
    static Geodetic3D FromDegrees(
      double longitudeDegrees,
      double latitudeDegrees,
      double heightMeters = 0.0)
    {
        // 注意：MathUtil 里的 degToRad 是 float 版本，直接使用会把 ~1e-6 度
        // （地表约 0.2m）的误差带进经纬度，并让 CartographicToCartesian /
        // CartesianToCartographic 的往返凭空出现漂移。这里用 double 常量换算。
        static const double kDegToRad = 0.01745329251994329576923690768489;
        return Geodetic3D(
                          longitudeDegrees * kDegToRad,
                          latitudeDegrees * kDegToRad,
                          heightMeters);
    }
    
    double Longitude() const
    {
        return longitude;
    }
    
    double Latitude() const
    {
        return latitude;
    }
    
    double Height() const
    {
        return height;
    }
    
public:
    double longitude;
    double latitude;
    double height;
};


EARTH_CORE_NAMESPACE_END

#endif /* GNX_MAP_ENGINE_EARTH_ENGINE_CORE_FEODETIC3D_INCLUDE_HJFJ */
