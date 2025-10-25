//
//  GeoTransform.h
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#ifndef GNX_MAP_ENGINE_GEO_TRANSFORM_INCLUDE_SDGJKFJG
#define GNX_MAP_ENGINE_GEO_TRANSFORM_INCLUDE_SDGJKFJG

#include <stdio.h>
#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

class GeoTransform
{
public:
    /**
    * @brief 计算从局部东北天的局部坐标系到椭球的固定参考系的变换矩阵
    *
    * Computes a 4x4 transformation matrix from a reference frame with an
    * east-north-up axes centered at the provided origin to the provided
    * ellipsoid's fixed reference frame. The local axes are defined as: <ul>
    *  <li>The `x` axis points in the local east direction.</li>
    *  <li>The `y` axis points in the local north direction.</li>
    *  <li>The `z` axis points in the direction of the ellipsoid surface normal
    * which passes through the position.</li>
    * </ul>
    *
    * @param origin 局部坐标系的原点坐标.
    * @param ellipsoid 用于计算的椭球 默认值: {@link Ellipsoid::WGS84}.
    * @return 变换矩阵
    */
    static Matrix4x4d eastNorthUpToFixedFrame(
        const Vector3d& origin,
        const Ellipsoid& ellipsoid) noexcept;
    
    /**
    * @brief 计算从局部东北天的局部坐标系到椭球的固定参考系的变换矩阵
    *
    * Computes a 4x4 transformation matrix from a reference frame with an
    * east-north-up axes centered at the provided origin to the provided
    * ellipsoid's fixed reference frame. The local axes are defined as: <ul>
    *  <li>The `x` axis points in the local east direction.</li>
    *  <li>The `y` axis points in the local north direction.</li>
    *  <li>The `z` axis points in the direction of the ellipsoid surface normal
    * which passes through the position.</li>
    * </ul>
    *
    * @param origin 局部坐标系的原点坐标.
    * @param ellipsoid 用于计算的椭球 默认值: {@link Ellipsoid::WGS84}.
    * @return 变换矩阵
    */
    static Matrix4x4d eastNorthUpToFixedFrame(
        const Geodetic3D& origin,
        const Ellipsoid& ellipsoid) noexcept;
};

EARTH_CORE_NAMESPACE_END

#endif /* GeoTransform_hpp */
