
#include "BoundingRegion.h"
#include "Geodetic3D.h"
#include "EllipsoidTangentPlane.h"
#include "GeoTransform.h"
#include <limits>

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

OrientedBoundingBoxd BoundingRegion::ComputeMeshBoundingBox(
	const GlobeRectangle& rectangle, const Ellipsoid& ellipsoid,
	uint32_t rows, uint32_t columns, const float* heights)
{
	const Matrix4x4d frame = GeoTransform::eastNorthUpToFixedFrame(
		rectangle.computeCenter(), ellipsoid);
	const Vector3d origin = frame.col(3).xyz();
	const Vector3d east = frame.col(0).xyz();
	const Vector3d north = frame.col(1).xyz();
	const Vector3d up = frame.col(2).xyz();
	const double infinity = std::numeric_limits<double>::infinity();
	Vector3d minimum(infinity, infinity, infinity);
	Vector3d maximum(-infinity, -infinity, -infinity);
	const double west = rectangle.getWest();
	const double eastLongitude = rectangle.getEast() < west
		? rectangle.getEast() + 2.0 * M_PI : rectangle.getEast();
	for (uint32_t row = 0; row < rows; ++row)
	{
		const double latitude = rectangle.getSouth() +
			(rectangle.getNorth() - rectangle.getSouth()) * row / (rows - 1);
		for (uint32_t column = 0; column < columns; ++column)
		{
			const double longitude = west + (eastLongitude - west) * column / (columns - 1);
			const double height = heights ? heights[(size_t)row * columns + column] : 0.0;
			const Vector3d offset = ellipsoid.CartographicToCartesian(
				Geodetic3D(longitude, latitude, height)) - origin;
			const Vector3d local(offset.DotProduct(east),
				offset.DotProduct(north), offset.DotProduct(up));
			minimum.x = std::min(minimum.x, local.x);
			minimum.y = std::min(minimum.y, local.y);
			minimum.z = std::min(minimum.z, local.z);
			maximum.x = std::max(maximum.x, local.x);
			maximum.y = std::max(maximum.y, local.y);
			maximum.z = std::max(maximum.z, local.z);
		}
	}
	// Vertices are stored relative to the tile origin as floats. Leave room
	// for conversion and camera matrix rounding at large world coordinates.
	constexpr double margin = 4.0;
	return fromPlaneExtents(origin, east, north, up,
		minimum.x - margin, maximum.x + margin,
		minimum.y - margin, maximum.y + margin,
		minimum.z - margin, maximum.z + margin);
}

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
                        plane.GetPointDistance(perimeterCartesianNW),
                        plane.GetPointDistance(perimeterCartesianSW));
		maxZ = maximumHeight; // Since the tangent plane touches the surface at height = 0, this is okay

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
    
    // 处理矩形宽度大于 π 的情况（环绕椭球体超过一半）。
    const bool fullyAboveEquator = rectangle.getSouth() > 0.0;
    const bool fullyBelowEquator = rectangle.getNorth() < 0.0;
    const double latitudeNearestToEquator =
      fullyAboveEquator   ? rectangle.getSouth()
      : fullyBelowEquator ? rectangle.getNorth()
                          : 0.0;
    const double centerLongitude = rectangle.computeCenter().longitude;
    
    // Plane is located at the rectangle's center longitude and the rectangle's
    // latitude that is closest to the equator. It rotates around the Z axis. This
    // results in a better fit than the obb approach for smaller rectangles, which
    // orients with the rectangle's center normal.
    Vector3d planeOrigin = ellipsoid.CartographicToCartesian(
                                                             Geodetic3D(centerLongitude, latitudeNearestToEquator, maximumHeight));
    planeOrigin.z = 0.0; // center the plane on the equator to simpify plane normal calculation
    const bool isPole = fabs(planeOrigin.x) < Epsilon14 && fabs(planeOrigin.y) < Epsilon14;
    const Vector3d planeNormal =
      !isPole ? planeOrigin.Normalize() : Vector3d(1.0, 0.0, 0.0);
    const Vector3d planeYAxis(0.0, 0.0, 1.0);
    const Vector3d planeXAxis = Vector3d::CrossProduct(planeNormal, planeYAxis);
    plane = Plane(planeOrigin, planeNormal);
    
    // Get the horizon point relative to the center. This will be the farthest
    // extent in the plane's X dimension.
    const Vector3d horizonCartesian =
      ellipsoid.CartographicToCartesian(Geodetic3D(
          centerLongitude + M_PI_2,
          latitudeNearestToEquator,
          maximumHeight));
    
    maxX = plane.ProjectPointOntoPlane(horizonCartesian).DotProduct(planeXAxis);
    minX = -maxX; // symmetrical
    
    // Get the min and max Y, using the height that will give the largest extent
    maxY = ellipsoid
             .CartographicToCartesian(Geodetic3D(
                 0.0,
                 rectangle.getNorth(),
                 fullyBelowEquator ? minimumHeight : maximumHeight))
             .z;
    minY = ellipsoid
             .CartographicToCartesian(Geodetic3D(
                 0.0,
                 rectangle.getSouth(),
                 fullyAboveEquator ? minimumHeight : maximumHeight))
             .z;
    const Vector3d farZ = ellipsoid.CartographicToCartesian(Geodetic3D(
      rectangle.getEast(),
      latitudeNearestToEquator,
      maximumHeight));
    minZ = plane.GetPointDistance(farZ);
    maxZ = 0.0; // plane origin starts at maxZ already

    // min and max are local to the plane axes
    return fromPlaneExtents(
      planeOrigin,
      planeXAxis,
      planeYAxis,
      planeNormal,
      minX,
      maxX,
      minY,
      maxY,
      minZ,
      maxZ);
}

EARTH_CORE_NAMESPACE_END
