#include "EllipsoidTangentPlane.h"
#include "GeoTransform.h"
#include "IntersectionTests.h"

EARTH_CORE_NAMESPACE_BEGIN

EllipsoidTangentPlane::EllipsoidTangentPlane(
	const Vector3d& origin,
	const Ellipsoid& ellipsoid)
	: EllipsoidTangentPlane(
		computeEastNorthUpToFixedFrame(origin, ellipsoid),
		ellipsoid) 
{
}

EllipsoidTangentPlane::EllipsoidTangentPlane(
	const Matrix4x4d& eastNorthUpToFixedFrame,
	const Ellipsoid& ellipsoid)
	: mEllipsoid(ellipsoid),
	mOrigin(eastNorthUpToFixedFrame.col(3).xyz()),
	mXAxis(eastNorthUpToFixedFrame.col(0).xyz()),
	mYAxis(eastNorthUpToFixedFrame.col(1).xyz()),
	mPlane(
		Vector3d(eastNorthUpToFixedFrame.col(2).xyz()),
		Vector3d(eastNorthUpToFixedFrame.col(3).xyz()))
{
}

Vector2d EllipsoidTangentPlane::projectPointToNearestOnPlane(const Vector3d& cartesian) const noexcept
{
	const Rayd ray(cartesian, this->mPlane.getNormal());

	Vector3d intersectionPoint;
	bool isIntersect = IntersectionTests::RayPlane(ray, this->mPlane, intersectionPoint);
	if (!isIntersect) 
	{
		isIntersect = IntersectionTests::RayPlane(-ray, this->mPlane, intersectionPoint);
		if (!isIntersect) 
		{
			intersectionPoint = cartesian;
		}
	}

	const Vector3d v = intersectionPoint - this->mOrigin;
	return Vector2d(mXAxis.DotProduct(v), mYAxis.DotProduct(v));
}

Matrix4x4d EllipsoidTangentPlane::computeEastNorthUpToFixedFrame(
	const Vector3d& origin,
	const Ellipsoid& ellipsoid) 
{
	Vector3d scaledOrigin = ellipsoid.ScaleToGeodeticSurface(origin);
	if (scaledOrigin == Vector3d(0, 0, 0)) 
	{
		throw std::invalid_argument(
			"The origin must not be near the center of the ellipsoid.");
	}

	return GeoTransform::eastNorthUpToFixedFrame(scaledOrigin, ellipsoid);
}

EARTH_CORE_NAMESPACE_END