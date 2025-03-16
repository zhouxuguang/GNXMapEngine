#include "EllipsoidTangentPlane.h"
#include "GeoTransform.h"

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
	mOrigin(eastNorthUpToFixedFrame[3].xyz()),
	mXAxis(eastNorthUpToFixedFrame[0].xyz()),
	mYAxis(eastNorthUpToFixedFrame[1].xyz()),
	mPlane(
		Vector3d(eastNorthUpToFixedFrame[3].xyz()),
		Vector3d(eastNorthUpToFixedFrame[2].xyz()))
{
}

Vector2d EllipsoidTangentPlane::projectPointToNearestOnPlane(const Vector3d& cartesian) const noexcept
{
	return Vector2d();
	/*const Ray ray(cartesian, this->mPlane.getNormal());

	std::optional<glm::dvec3> intersectionPoint =
		IntersectionTests::rayPlane(ray, this->_plane);
	if (!intersectionPoint) {
		intersectionPoint = IntersectionTests::rayPlane(-ray, this->_plane);
		if (!intersectionPoint) {
			intersectionPoint = cartesian;
		}
	}

	const glm::dvec3 v = intersectionPoint.value() - this->_origin;
	return glm::dvec2(glm::dot(this->_xAxis, v), glm::dot(this->_yAxis, v));*/
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