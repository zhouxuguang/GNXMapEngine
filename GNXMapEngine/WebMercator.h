//
//  WebMercator.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#ifndef WebMercator_hpp
#define WebMercator_hpp

#include <math.h>
#include "Runtime/MathUtil/include/Vector2.h"

using namespace mathutil;

const static    int     tileSize            =   256;   //瓦片的像素大小
const static    double  initialResolution   =   2 * M_PI * 6378137 / tileSize;   //最粗糙等级每个像素对应的世界范围
const static    double  originShift         =   2 * M_PI * 6378137 / 2.0;

class WebMercator
{
public:
    WebMercator()
    {}

    static  double  lonToMeter(double lon)
    {
        return  lon * originShift / 180.0 ;
    }
    
    static  double  latToMeter(double lat)
    {
        double  my =    log( tan((90 + lat) * M_PI / 360.0 )) / (M_PI / 180.0);
        return  my =    my * originShift / 180.0;
    }
    
    /**
    *   经纬度转换为米
    */
    static  Vector2d lonLatToMeters(double lon, double lat)
    {
        // Converts given lat/lon in WGS84 Datum to XY in Spherical Mercator EPSG:900913
        double  mx =    lon * originShift / 180.0 ;
        double  my =    log( tan((90 + lat) * M_PI / 360.0 )) / (M_PI / 180.0);
        my =    my * originShift / 180.0;
        return  Vector2d(mx, my);
    }
    
    /**
    *   米转换为经纬度
    */
    static  Vector2d metersToLontLat(int mx, int my)
    {
        double  lon =   (mx / originShift) * 180.0;
        double  lat =   (my / originShift) * 180.0;
                lat =   180 / M_PI * (2 * atan( exp( lat * M_PI / 180.0)) - M_PI / 2.0);
        return  Vector2d(lon, lat);
    }

    // 根据级别计算分辨率
    static  double  resolution(int zoom)
    {
        return  initialResolution / (pow(2, double(zoom)));
    }
    
    /**
    * 经度计算瓦片编号
    */
    static int long2tilex(double lon, int z)
    {
        return (int)(floor((lon + 180.0) / 360.0 * pow(2.0, z)));
    }
    
    /**
    *   纬度计算瓦片编号
    */
    static int lat2tiley(double lat, int z)
    {
        return  (int)(floor((1.0 - log( tan(lat * M_PI/180.0) + 1.0 / cos(lat * M_PI/180.0)) / M_PI) / 2.0 * pow(2.0, z)));
    }
    
    /**
    *
    */
    static  double  tilex2long(int x, int z)
    {
        return x / pow(2.0, z) * 360.0 - 180;
    }
    /**
    *
    */
    static  double  tiley2lat(int y, int z)
    {
        double n = M_PI - 2.0 * M_PI * y / pow(2.0, z);
        return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
    }

    static  Vector2d tileToWorld(Vector2i tileID, int z)
    {
        double  dLong   =   tilex2long(tileID.x, z);
        double  dLat    =   tiley2lat(tileID.y, z);

        return  lonLatToMeters(dLong,dLat);
    }

    /**
    *
    */
    static Vector2i getKey(unsigned level, double rLong, double rLat)
    {
        int     xTile   =    long2tilex(rLong,level);
        int     yTile   =    lat2tiley(rLat,level);
        return  Vector2i(xTile, yTile);
    }

    /**
    *
    */
    static Vector2i getKeyByMeter(unsigned level, double x, double y)
    {
        Vector2d lonLat  =   metersToLontLat(x,y);
        int     xTile   =   long2tilex(lonLat.x, level);
        int     yTile   =   lat2tiley(lonLat.y,level);
        return  Vector2i(xTile, yTile);
    }
};

#endif /* WebMercator_hpp */
