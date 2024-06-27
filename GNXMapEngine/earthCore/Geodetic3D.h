//
//  Geodetic3D.h
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#ifndef Geodetic3D_hpp
#define Geodetic3D_hpp

#include <stdio.h>
#include "EarthEngineDefine.h"

EARTH_CORE_NAMESPACE_BEGIN


// 弧度表示的地理坐标，经纬度
class Geodetic3D
{
public:
    Geodetic3D(double longitude, double latitude, double height)
    {
        this->longitude = longitude;
        this->latitude = latitude;
        this->height = height;
    }
    
    Geodetic3D(double longitude, double latitude)
    {
        this->longitude = longitude;
        this->latitude = latitude;
        this->height = 0;
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

#endif /* Geodetic3D_hpp */
