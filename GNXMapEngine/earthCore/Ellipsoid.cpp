//
//  Ellipsoid.cpp
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

const Ellipsoid Ellipsoid::WGS84 = Ellipsoid(6378137.0, 6378137.0, 6356752.3142451793);
const Ellipsoid Ellipsoid::UnitSphere = Ellipsoid(1.0, 1.0, 1.0);

Ellipsoid::Ellipsoid(double x, double y, double z) : Ellipsoid(Vector3d(x, y, z))
{
}

Ellipsoid::Ellipsoid(const Vector3d& radii)
{
    mAxisLength = radii;
    
    mRadii = radii;
    mRadiiSquared = Vector3d(radii.x * radii.x, radii.y * radii.y, radii.z * radii.z);
    mOneOverRadii = Vector3d(1.0 / radii.x, 1.0 / radii.y, 1.0 / radii.z);
    mOneOverRadiiSquared = Vector3d(
                1.0 / (radii.x * radii.x),
                1.0 / (radii.y * radii.y),
                1.0 / (radii.z * radii.z));
}

Vector3d Ellipsoid::GetAxis() const
{
    return mAxisLength;
}

Vector3d Ellipsoid::GetOneOverRadiiSquared() const
{
    return mOneOverRadiiSquared;
}

Vector3d Ellipsoid::GeodeticSurfaceNormal(const Vector3d &positionOnEllipsoid) const
{
    Vector3d vecNormal = positionOnEllipsoid.Multiply(mOneOverRadiiSquared);
    vecNormal.Normalize();
    
    return vecNormal;
}

Vector3d Ellipsoid::GeodeticSurfaceNormal(const Geodetic3D& geodetic) const
{
    double cosLatitude = cos(geodetic.Latitude());
    
    Vector3d vecNormal = Vector3d(
                        cosLatitude * cos(geodetic.Longitude()),
                        cosLatitude * sin(geodetic.Longitude()),
                        sin(geodetic.Latitude()));
    
    vecNormal.Normalize();
    return vecNormal;
}

Vector3d Ellipsoid::CartographicToCartesian(const Geodetic3D& cartographic) const
{
    Vector3d n = this->GeodeticSurfaceNormal(cartographic);
    Vector3d k = this->mRadiiSquared * n;
    const double gamma = sqrt(n.DotProduct(k));
    k /= gamma;
    n *= cartographic.height;
    return k + n;
}

// 笛卡尔坐标转地理坐标
Geodetic3D Ellipsoid::CartesianToCartographic(const Vector3d& cartesian) const
{
    Vector3d p = this->ScaleToGeodeticSurface(cartesian);

    const Vector3d n = this->GeodeticSurfaceNormal(p);
    const Vector3d h = cartesian - p;

    const double longitude = atan2(n.y, n.x);
    const double latitude = asin(n.z);
    
    double sign = h.DotProduct(cartesian);
    if (sign >= 0.0)
    {
        sign = 1.0;
    }
    else
    {
        sign = -1.0;
    }
    
    const double height = sign * h.Length();

    return Geodetic3D(longitude, latitude, height);
}

Vector3d Ellipsoid::ScaleToGeodeticSurface(const Vector3d& cartesian) const
{
    const double positionX = cartesian.x;
    const double positionY = cartesian.y;
    const double positionZ = cartesian.z;

    const double oneOverRadiiX = this->mOneOverRadii.x;
    const double oneOverRadiiY = this->mOneOverRadii.y;
    const double oneOverRadiiZ = this->mOneOverRadii.z;

    const double x2 = positionX * positionX * oneOverRadiiX * oneOverRadiiX;
    const double y2 = positionY * positionY * oneOverRadiiY * oneOverRadiiY;
    const double z2 = positionZ * positionZ * oneOverRadiiZ * oneOverRadiiZ;

    // Compute the squared ellipsoid norm.
    const double squaredNorm = x2 + y2 + z2;
    const double ratio = sqrt(1.0 / squaredNorm);

    // As an initial approximation, assume that the radial intersection is the
    // projection point.
    Vector3d intersection = cartesian * ratio;

    // If the position is near the center, the iteration will not converge.
//    if (squaredNorm < this->_centerToleranceSquared) {
//        return !std::isfinite(ratio) ? std::optional<glm::dvec3>() : intersection;
//    }

    const double oneOverRadiiSquaredX = this->mOneOverRadiiSquared.x;
    const double oneOverRadiiSquaredY = this->mOneOverRadiiSquared.y;
    const double oneOverRadiiSquaredZ = this->mOneOverRadiiSquared.z;

    // Use the gradient at the intersection point in place of the true unit
    // normal. The difference in magnitude will be absorbed in the multiplier.
    const Vector3d gradient(
      intersection.x * oneOverRadiiSquaredX * 2.0,
      intersection.y * oneOverRadiiSquaredY * 2.0,
      intersection.z * oneOverRadiiSquaredZ * 2.0);

    // Compute the initial guess at the normal vector multiplier, lambda.
    double lambda = (1.0 - ratio) * cartesian.Length() / (0.5 * gradient.Length());
    double correction = 0.0;
    
    static constexpr double Epsilon12 = 1e-12;

    double func;
    double denominator;
    double xMultiplier;
    double yMultiplier;
    double zMultiplier;
    double xMultiplier2;
    double yMultiplier2;
    double zMultiplier2;
    double xMultiplier3;
    double yMultiplier3;
    double zMultiplier3;

    do {
        lambda -= correction;

        xMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquaredX);
        yMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquaredY);
        zMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquaredZ);

        xMultiplier2 = xMultiplier * xMultiplier;
        yMultiplier2 = yMultiplier * yMultiplier;
        zMultiplier2 = zMultiplier * zMultiplier;

        xMultiplier3 = xMultiplier2 * xMultiplier;
        yMultiplier3 = yMultiplier2 * yMultiplier;
        zMultiplier3 = zMultiplier2 * zMultiplier;

        func = x2 * xMultiplier2 + y2 * yMultiplier2 + z2 * zMultiplier2 - 1.0;

        // "denominator" here refers to the use of this expression in the velocity
        // and acceleration computations in the sections to follow.
        denominator = x2 * xMultiplier3 * oneOverRadiiSquaredX +
                      y2 * yMultiplier3 * oneOverRadiiSquaredY +
                      z2 * zMultiplier3 * oneOverRadiiSquaredZ;

        const double derivative = -2.0 * denominator;

        correction = func / derivative;
    } while (fabs(func) > Epsilon12);

    return Vector3d(
      positionX * xMultiplier,
      positionY * yMultiplier,
      positionZ * zMultiplier);
}

EARTH_CORE_NAMESPACE_END
