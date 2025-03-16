
#include "BoundingRegion.h"
#include "Geodetic3D.h"
#include "EllipsoidTangentPlane.h"

EARTH_CORE_NAMESPACE_BEGIN

BoundingRegion::BoundingRegion(const GlobeRectangle& rectangle, double minimumHeight, double maximumHeight, const Ellipsoid& ellipsoid)
	: mRectangle(rectangle),
	mBoundingBox(computeBoundingBox(rectangle, minimumHeight, maximumHeight, ellipsoid))
{
}

namespace 
{
	OrientedBoundingBoxd fromPlaneExtents(
		const Vector3d& planeOrigin,
		const Vector3d& planeXAxis,
		const Vector3d& planeYAxis,
		const Vector3d& planeZAxis,
		double minimumX,
		double maximumX,
		double minimumY,
		double maximumY,
		double minimumZ,
		double maximumZ) noexcept 
	{
		Matrix3x3d halfAxes(planeXAxis.x, planeYAxis.x, planeZAxis.x,
			planeXAxis.y, planeYAxis.y, planeZAxis.y,
			planeXAxis.z, planeYAxis.z, planeZAxis.z);

		const Vector3d centerOffset(
			(minimumX + maximumX) / 2.0,
			(minimumY + maximumY) / 2.0,
			(minimumZ + maximumZ) / 2.0);

		const Vector3d scale(
			(maximumX - minimumX) / 2.0,
			(maximumY - minimumY) / 2.0,
			(maximumZ - minimumZ) / 2.0);

		Vector3d xAxis = planeXAxis * scale.x;
		Vector3d yAxis = planeYAxis * scale.y;
		Vector3d zAxis = planeZAxis * scale.z;

		const Matrix3x3d scaledHalfAxes(
			xAxis.x, yAxis.x, zAxis.x,
			xAxis.y, yAxis.y, zAxis.y,
			xAxis.z, yAxis.z, zAxis.z);

		return OrientedBoundingBoxd(
			planeOrigin + (halfAxes * centerOffset),
			scaledHalfAxes);
	}
} // namespace

OrientedBoundingBoxd BoundingRegion::computeBoundingBox(const GlobeRectangle& rectangle, double minimumHeight, double maximumHeight, const Ellipsoid& ellipsoid)
{
	// 这里假设是旋转椭球体，因为非旋转椭球计算很复杂

	double minX, maxX, minY, maxY, minZ, maxZ;
	Planed plane(Vector3d(0.0, 0.0, 1.0), 0.0);

	double width = rectangle.computeWidth();
	// 处理经度范围小于π的情况
	if (width <= M_PI)
	{
		// 边界框将与矩形中心处的切平面对齐
		const Geodetic3D tangentPointCartographic = rectangle.computeCenter();
		const Vector3d tangentPoint = ellipsoid.CartographicToCartesian(tangentPointCartographic);
		const EllipsoidTangentPlane tangentPlane(tangentPoint, ellipsoid);
		plane = tangentPlane.getPlane();

		// 若矩形横跨赤道，则坐标系将改为与赤道对齐（因为赤道处向外突出最远）
		const double lonCenter = tangentPointCartographic.longitude;
		const double latCenter =
			rectangle.getSouth() < 0.0 && rectangle.getNorth() > 0.0
			? 0.0
			: tangentPointCartographic.latitude;

		// Compute XY extents using the rectangle at maximum height
		const Geodetic3D perimeterCartographicNC(
			lonCenter,
			rectangle.getNorth(),
			maximumHeight);
		Geodetic3D perimeterCartographicNW(
			rectangle.getWest(),
			rectangle.getNorth(),
			maximumHeight);
		const Geodetic3D perimeterCartographicCW(
			rectangle.getWest(),
			latCenter,
			maximumHeight);
		Geodetic3D perimeterCartographicSW(
			rectangle.getWest(),
			rectangle.getSouth(),
			maximumHeight);
		const Geodetic3D perimeterCartographicSC(
			lonCenter,
			rectangle.getSouth(),
			maximumHeight);

		const Vector3d perimeterCartesianNC =
			ellipsoid.CartographicToCartesian(perimeterCartographicNC);
		Vector3d perimeterCartesianNW =
			ellipsoid.CartographicToCartesian(perimeterCartographicNW);
		const Vector3d perimeterCartesianCW =
			ellipsoid.CartographicToCartesian(perimeterCartographicCW);
		Vector3d perimeterCartesianSW =
			ellipsoid.CartographicToCartesian(perimeterCartographicSW);
		const Vector3d perimeterCartesianSC =
			ellipsoid.CartographicToCartesian(perimeterCartographicSC);

		const Vector2d perimeterProjectedNC =
			tangentPlane.projectPointToNearestOnPlane(perimeterCartesianNC);
		const Vector2d perimeterProjectedNW =
			tangentPlane.projectPointToNearestOnPlane(perimeterCartesianNW);
		const Vector2d perimeterProjectedCW =
			tangentPlane.projectPointToNearestOnPlane(perimeterCartesianCW);
		const Vector2d perimeterProjectedSW =
			tangentPlane.projectPointToNearestOnPlane(perimeterCartesianSW);
		const Vector2d perimeterProjectedSC =
			tangentPlane.projectPointToNearestOnPlane(perimeterCartesianSC);

		minX = std::min(
			std::min(perimeterProjectedNW.x, perimeterProjectedCW.x),
			perimeterProjectedSW.x);

		maxX = -minX; // symmetrical

		maxY = std::max(perimeterProjectedNW.y, perimeterProjectedNC.y);
		minY = std::min(perimeterProjectedSW.y, perimeterProjectedSC.y);

		// Compute minimum Z using the rectangle at minimum height, since it will be
		// deeper than the maximum height
		perimeterCartographicNW.height = perimeterCartographicSW.height =
			minimumHeight;
		perimeterCartesianNW =
			ellipsoid.CartographicToCartesian(perimeterCartographicNW);
		perimeterCartesianSW =
			ellipsoid.CartographicToCartesian(perimeterCartographicSW);

		minZ = std::min(
			plane.getPointDistance(perimeterCartesianNW),
			plane.getPointDistance(perimeterCartesianSW));
		maxZ = maximumHeight; // Since the tangent plane touches the surface at
		// height = 0, this is okay

		// Esure our box is at least a millimeter in each direction to avoid
		// problems with degenerate or nearly-degenerate bounding regions.
		const double oneMillimeter = 0.001;
		if (maxX - minX < oneMillimeter) 
		{
			minX -= oneMillimeter * 0.5;
			maxX += oneMillimeter * 0.5;
		}
		if (maxY - minY < oneMillimeter) 
		{
			minY -= oneMillimeter * 0.5;
			maxY += oneMillimeter * 0.5;
		}
		if (maxZ - minZ < oneMillimeter) 
		{
			minZ -= oneMillimeter * 0.5;
			maxZ += oneMillimeter * 0.5;
		}

		return fromPlaneExtents(
			tangentPlane.getOrigin(),
			tangentPlane.getXAxis(),
			tangentPlane.getYAxis(),
			tangentPlane.getZAxis(),
			minX,
			maxX,
			minY,
			maxY,
			minZ,
			maxZ);
	}

	Vector3d center;
	Matrix3x3d halfAxes;
	return OrientedBoundingBoxd(center, halfAxes);
}

EARTH_CORE_NAMESPACE_END

