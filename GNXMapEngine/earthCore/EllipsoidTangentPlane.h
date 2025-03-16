//
//  EllipsoidTangentPlane.h
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#ifndef EARTH_ENGINE_ELLIPSOID_TANGENTPLANE_INCLUDE_H
#define EARTH_ENGINE_ELLIPSOID_TANGENTPLANE_INCLUDE_H

#include "EarthEngineDefine.h"
#include "Geodetic3D.h"
#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

// 椭球体切平面
class EllipsoidTangentPlane 
{
public:
    /**
     * @brief Creates a new instance.
     *
     * @param origin The origin, in cartesian coordinates.
     * @param ellipsoid The ellipsoid. Default value: {@link Ellipsoid::WGS84}.
     * @throws An `std::invalid_argument` if the given origin is at the
     * center of the ellipsoid.
     */
    EllipsoidTangentPlane(
        const Vector3d& origin,
        const Ellipsoid& ellipsoid);

    /**
     * @brief Creates a new instance.
     *
     * @param eastNorthUpToFixedFrame A transform that was computed with
     * {@link GlobeTransforms::eastNorthUpToFixedFrame}.
     * @param ellipsoid The ellipsoid. Default value: {@link Ellipsoid::WGS84}.
     */
    EllipsoidTangentPlane(
        const Matrix4x4d& eastNorthUpToFixedFrame,
        const Ellipsoid& ellipsoid);

    /**
     * @brief Returns the {@link Ellipsoid}.
     */
    const Ellipsoid& getEllipsoid() const noexcept { return this->mEllipsoid; }

    /**
     * @brief Returns the origin, in cartesian coordinates.
     */
    const Vector3d& getOrigin() const noexcept { return this->mOrigin; }

    /**
     * @brief Returns the x-axis of this pl.ane
     */
    const Vector3d& getXAxis() const noexcept { return this->mXAxis; }

    /**
     * @brief Returns the y-axis of this plane.
     */
    const Vector3d& getYAxis() const noexcept { return this->mYAxis; }

    /**
     * @brief Returns the z-axis (i.e. the normal) of this plane.
     */
    const Vector3d& getZAxis() const noexcept 
    {
        return this->mPlane.getNormal();
    }

    /**
     * @brief Returns a representation of this plane.
     * 
     */
    const Planed& getPlane() const noexcept 
    {
        return this->mPlane;
    }

    /**
     * @brief Computes the position of the projection of the given point on this
     * plane.
     *
     * Projects the given point on this plane, along the normal. The result will
     * be a 2D point, referring to the local coordinate system of the plane that
     * is given by the x- and y-axis.
     *
     * @param cartesian The point in cartesian coordinates.
     * @return The 2D representation of the point on the plane that is closest to
     * the given position.
     */
    Vector2d projectPointToNearestOnPlane(const Vector3d& cartesian) const noexcept;

private:
    /**
     * Computes the matrix that is used for the constructor (if the origin
     * and ellipsoid are given), but throws an `std::invalid_argument` if
     * the origin is at the center of the ellipsoid.
     */
    static Matrix4x4d computeEastNorthUpToFixedFrame(
        const Vector3d& origin,
        const Ellipsoid& ellipsoid);

    Ellipsoid mEllipsoid;
    Vector3d mOrigin;
    Vector3d mXAxis;
    Vector3d mYAxis;
    Planed mPlane;
};

EARTH_CORE_NAMESPACE_END

#endif