//
//  WebMercator.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#ifndef WebMercator_hpp
#define WebMercator_hpp

#include <math.h>
#include <MathUtil/Vector2.h>

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
};

#endif /* WebMercator_hpp */
