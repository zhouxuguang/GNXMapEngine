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
        return Geodetic3D(
                          degToRad(longitudeDegrees),
                          degToRad(latitudeDegrees),
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
