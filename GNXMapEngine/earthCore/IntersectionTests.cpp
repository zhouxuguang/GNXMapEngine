//
//  IntersectionTests.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/7/4.
//

#include "IntersectionTests.h"

EARTH_CORE_NAMESPACE_BEGIN

bool IntersectionTests::RayEllipsoid(const Ray& ray, const Ellipsoid& ellipsoid, Vector3d& intersectPoint) noexcept
{
    Vector3d inverseRadii = ellipsoid.GetOneOverRadii();

    Vector3d origin = Vector3d(ray.getOrigin().x, ray.getOrigin().y, ray.getOrigin().z);
    Vector3d direction = Vector3d(ray.getDirection().x, ray.getDirection().y, ray.getDirection().z);

    Vector3d q = inverseRadii * origin;
    Vector3d w = inverseRadii * direction;

    const double q2 = pow(q.Length(), 2);
    const double qw = q.DotProduct(w);

    double difference, w2, product, discriminant, temp;
    
    if (q2 > 1.0) 
    {
        // Outside ellipsoid.
        if (qw >= 0.0)
        {
            // Looking outward or tangent (0 intersections).
            return false;
        }
        
        // qw < 0.0.
        const double qw2 = qw * qw;
        difference = q2 - 1.0; // Positively valued.
        w2 = pow(w.Length(), 2);
        product = w2 * difference;
        
        if (qw2 < product) 
        {
            // Imaginary roots (0 intersections).
            return false;
        }
        if (qw2 > product) 
        {
            // Distinct roots (2 intersections).
            discriminant = qw * qw - product;
            temp = -qw + sqrt(discriminant); // Avoid cancellation.
            const double root0 = temp / w2;
            const double root1 = difference / temp;
            if (root0 < root1) 
            {
                intersectPoint = origin + direction * root0;
                return true;
            }
            
            intersectPoint = origin + direction * root1;
            return true;
        }
        // qw2 == product.  Repeated roots (2 intersections).
        const double root = sqrt(difference / w2);
        intersectPoint = origin + direction * root;
        return true;
    }
    
    if (q2 < 1.0) 
    {
        // 在椭球内部，两个交点
        return false;
    }

    // q2 == 1.0. On ellipsoid.
    if (qw < 0.0) 
    {
        // Looking inward.
        intersectPoint = origin;
        return true;
    }
    // qw >= 0.0.  Looking outward or tangent.
    return false;
}

EARTH_CORE_NAMESPACE_END
