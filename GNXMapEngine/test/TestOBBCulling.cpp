#include "earthCore/BoundingRegion.h"
#include "earthCore/EarthCameraPose.h"
#include "earthCore/GeoTransform.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>

using namespace earthcore;

namespace
{
constexpr uint32_t kGrid = 65;
constexpr double kRadians = M_PI / 180.0;
int failures = 0;

void Check(const char* label, bool result)
{
	if (!result)
	{
		std::printf("FAIL: %s\n", label);
		++failures;
	}
}

struct Tile
{
	OrientedBoundingBoxd obb;
	AxisAlignedBoxd aabb;
	std::vector<Vector3d> vertices;
};

Tile MakeTile(const GlobeRectangle& rectangle, const std::vector<float>& heights)
{
	const Ellipsoid& earth = Ellipsoid::WGS84;
	const OrientedBoundingBoxd obb = BoundingRegion::ComputeMeshBoundingBox(
		rectangle, earth, kGrid, kGrid, heights.empty() ? nullptr : heights.data());
	const Vector3d southwest = earth.CartographicToCartesian(
		Geodetic3D(rectangle.getWest(), rectangle.getSouth(), 0.0));
	std::vector<Vector3d> vertices;
	vertices.reserve(kGrid * kGrid);
	for (uint32_t row = 0; row < kGrid; ++row)
	{
		const double latitude = rectangle.getSouth() +
			(rectangle.getNorth() - rectangle.getSouth()) * row / (kGrid - 1);
		for (uint32_t column = 0; column < kGrid; ++column)
		{
			const double longitude = rectangle.getWest() +
			(rectangle.getEast() - rectangle.getWest()) * column / (kGrid - 1);
			const double height = heights.empty() ? 0.0 : heights[row * kGrid + column];
			const Vector3d world = earth.CartographicToCartesian(
				Geodetic3D(longitude, latitude, height));
			// Match the float relative positions stored by DemMeshData::FillVertex.
			const Vector3d relative = world - southwest;
			vertices.emplace_back(southwest.x + static_cast<float>(relative.x),
				southwest.y + static_cast<float>(relative.y),
				southwest.z + static_cast<float>(relative.z));
		}
	}
	return {obb, obb.ToAxisAligned(), std::move(vertices)};
}

bool Contains(const OrientedBoundingBoxd& obb, const Vector3d& point)
{
	const Vector3d offset = point - obb.mCenter;
	for (int axis = 0; axis < 3; ++axis)
	{
		const Vector3d halfAxis = obb.mHalfAxes.col(axis);
		const double length = halfAxis.Length();
		if (std::abs(offset.DotProduct(halfAxis) / length) > length + 1e-6)
			return false;
	}
	return true;
}

Frustumd MakeFrustum(double eyeLongitude, double eyeLatitude, double altitude,
						double targetLongitude, double targetLatitude)
{
	const Ellipsoid& earth = Ellipsoid::WGS84;
	const Vector3d eye = earth.CartographicToCartesian(
		Geodetic3D(eyeLongitude * kRadians, eyeLatitude * kRadians, altitude));
	const Vector3d target = earth.CartographicToCartesian(
		Geodetic3D(targetLongitude * kRadians, targetLatitude * kRadians, 0));
	const Vector3d up = EarthCameraPose::LookUpForLookAt(eye, target, earth, 0.0);
	const Matrix4x4d preciseView = Matrix4x4d::CreateLookAt(eye, target, up);
	Matrix4x4f view;
	for (int row = 0; row < 4; ++row)
		for (int column = 0; column < 4; ++column)
			view[row][column] = static_cast<float>(preciseView[row][column]);
	const Matrix4x4f projection = Matrix4x4f::CreateInfiniteReverseZPerspective(60.0f, 16.0f / 9.0f, 10.0f);
	const Matrix4x4f floatViewProjection = projection * view;
	Matrix4x4d viewProjection;
	for (int row = 0; row < 4; ++row)
		for (int column = 0; column < 4; ++column)
			viewProjection[row][column] = floatViewProjection[row][column];
	Frustumd frustum;
	frustum.InitFrustum(viewProjection);
	return frustum;
}

bool HasSeparatingPlaneForVertices(const Frustumd& frustum, const Tile& tile)
{
	const Vector4d* planes = frustum.GetPlanes();
	for (int planeIndex = 0; planeIndex < kPlaneFrustumNum; ++planeIndex)
	{
		const Vector4d& plane = planes[planeIndex];
		bool allOutside = true;
		for (const Vector3d& vertex : tile.vertices)
		{
			if (plane.x * vertex.x + plane.y * vertex.y + plane.z * vertex.z + plane.w >= -1e-6)
			{
				allOutside = false;
				break;
			}
		}
		if (allOutside)
			return true;
	}
	return false;
}

bool CornerReference(const Frustumd& frustum, const OrientedBoundingBoxd& obb)
{
	const Vector4d* planes = frustum.GetPlanes();
	for (int planeIndex = 0; planeIndex < kPlaneFrustumNum; ++planeIndex)
	{
		const Vector4d& plane = planes[planeIndex];
		double maximum = -std::numeric_limits<double>::infinity();
		for (int x : {-1, 1})
			for (int y : {-1, 1})
				for (int z : {-1, 1})
				{
					const Vector3d corner = obb.mCenter + obb.mHalfAxes.col(0) * x +
						obb.mHalfAxes.col(1) * y + obb.mHalfAxes.col(2) * z;
					maximum = std::max(maximum,
						plane.x * corner.x + plane.y * corner.y + plane.z * corner.z + plane.w);
				}
		if (maximum < 0.0)
			return false;
	}
	return true;
}
} // namespace

int main()
{
	// Both overloads use the same contract: inside/intersecting is true.
	Frustumd unitFrustum;
	unitFrustum.InitFrustum(Matrix4x4d());
	const AxisAlignedBoxd inside(Vector3d(-0.1, -0.1, 0.4), Vector3d(0.1, 0.1, 0.6));
	const AxisAlignedBoxd outside(Vector3d(2.0, -0.1, 0.4), Vector3d(2.2, 0.1, 0.6));
	const AxisAlignedBoxd crossing(Vector3d(0.9, -0.1, 0.4), Vector3d(1.1, 0.1, 0.6));
	for (const AxisAlignedBoxd& box : {inside, outside, crossing})
	{
		Check("AABB/OBB overloads agree for axis-aligned boxes",
			unitFrustum.IsBoxInFrustum(box) ==
			unitFrustum.IsOBBInFrustum(OrientedBoundingBoxd::FromAxisAligned(box)));
	}
	Check("OBB inside returns true", unitFrustum.IsOBBInFrustum(
		OrientedBoundingBoxd::FromAxisAligned(inside)));
	Check("OBB outside returns false", !unitFrustum.IsOBBInFrustum(
		OrientedBoundingBoxd::FromAxisAligned(outside)));
	Check("OBB crossing returns true", unitFrustum.IsOBBInFrustum(
		OrientedBoundingBoxd::FromAxisAligned(crossing)));
	const AxisAlignedBoxd touching(Vector3d(1.0, -0.1, 0.4), Vector3d(2.0, 0.1, 0.6));
	Check("OBB touching frustum plane returns true", unitFrustum.IsOBBInFrustum(
		OrientedBoundingBoxd::FromAxisAligned(touching)));

	std::mt19937_64 boxRandom(0x43b2e51);
	std::uniform_real_distribution<double> centerCoordinate(-3.0, 3.0);
	std::uniform_real_distribution<double> halfLength(0.01, 2.0);
	std::uniform_real_distribution<double> angle(-M_PI, M_PI);
	for (int i = 0; i < 10000; ++i)
	{
		const double theta = angle(boxRandom);
		const double cosine = std::cos(theta);
		const double sine = std::sin(theta);
		const double xLength = halfLength(boxRandom);
		const double yLength = halfLength(boxRandom);
		const double zLength = halfLength(boxRandom);
		const Matrix3x3d axes(cosine * xLength, -sine * yLength, 0.0,
			sine * xLength, cosine * yLength, 0.0,
			0.0, 0.0, zLength);
		const OrientedBoundingBoxd obb(Vector3d(centerCoordinate(boxRandom),
			centerCoordinate(boxRandom), centerCoordinate(boxRandom)), axes);
		Check("10k rotated OBBs match eight-corner plane reference",
			unitFrustum.IsOBBInFrustum(obb) == CornerReference(unitFrustum, obb));
	}

	std::mt19937_64 random(0x0bb2026);
	std::uniform_real_distribution<double> lon(-175.0, 175.0);
	std::uniform_real_distribution<double> lat(-88.0, 88.0);
	std::uniform_real_distribution<double> elevation(-4000.0, 9000.0);
	std::vector<Tile> tiles;
	// Include the two half-globe roots, polar tiles, small tiles, and spiky DEMs.
	tiles.push_back(MakeTile(GlobeRectangle(-M_PI, -M_PI_2, 0.0, M_PI_2), {}));
	tiles.push_back(MakeTile(GlobeRectangle(0.0, -M_PI_2, M_PI, M_PI_2), {}));
	for (int i = 0; i < 240; ++i)
	{
		const double centerLon = lon(random) * kRadians;
		const double centerLat = lat(random) * kRadians;
		const double span = (i % 4 == 0 ? 12.0 : i % 4 == 1 ? 1.0 : i % 4 == 2 ? 0.1 : 0.01) * kRadians;
		const GlobeRectangle rectangle(std::max(-M_PI, centerLon - span),
			std::max(-M_PI_2, centerLat - span),
			std::min(M_PI, centerLon + span),
			std::min(M_PI_2, centerLat + span));
		std::vector<float> heights;
		if (i % 2 == 0)
		{
			heights.resize(kGrid * kGrid);
			for (float& height : heights)
				height = static_cast<float>(elevation(random));
		}
		tiles.push_back(MakeTile(rectangle, heights));
	}

	uint64_t verticesChecked = 0;
	for (const Tile& tile : tiles)
	{
		for (const Vector3d& vertex : tile.vertices)
		{
			if (!Contains(tile.obb, vertex))
			{
				Check("OBB contains every rendered vertex", false);
				break;
			}
			++verticesChecked;
		}
	}
	std::uniform_real_distribution<double> cameraAltitude(1000.0, 20000000.0);
	uint64_t randomFrustumChecks = 0;
	for (int cameraIndex = 0; cameraIndex < 80; ++cameraIndex)
	{
		const double longitude = lon(random);
		const double latitude = lat(random);
		const Frustumd frustum = MakeFrustum(longitude, latitude,
			cameraAltitude(random), longitude + (cameraIndex % 3 == 0 ? 10.0 : 0.0), latitude);
		for (const Tile& tile : tiles)
		{
			const bool aabbVisible = frustum.IsBoxInFrustum(tile.aabb);
			const bool obbVisible = frustum.IsOBBInFrustum(tile.obb);
			Check("random frustum: OBB visibility is subset of AABB", !obbVisible || aabbVisible);
			if (!obbVisible)
				Check("random frustum: no mesh vertex survives a rejecting plane",
					HasSeparatingPlaneForVertices(frustum, tile));
			++randomFrustumChecks;
		}
	}

	struct Camera { const char* name; double lon, lat, altitude, targetLon, targetLat; };
	const Camera cameras[] = {
		{"nadir", 0, 0, 1000000, 0, 0},
		{"high oblique", 0, 0, 6400000, 55, 0},
		{"low oblique", 110, 23, 10000, 110.5, 23},
		{"polar", 40, 78, 500000, 40, 78},
		{"below horizon", -95, 40, 200000, -85, 15},
	};
	std::printf("%-16s %12s %12s %12s %12s\n", "camera", "AABB keep", "OBB keep", "AABB us", "OBB us");
	volatile uint64_t benchmarkSink = 0;
	for (const Camera& camera : cameras)
	{
		const Frustumd frustum = MakeFrustum(camera.lon, camera.lat, camera.altitude,
			camera.targetLon, camera.targetLat);
		int aabbKeep = 0, obbKeep = 0;
		for (const Tile& tile : tiles)
		{
			const bool aabbVisible = frustum.IsBoxInFrustum(tile.aabb);
			const bool obbVisible = frustum.IsOBBInFrustum(tile.obb);
			aabbKeep += aabbVisible;
			obbKeep += obbVisible;
			Check("OBB visibility is subset of AABB", !obbVisible || aabbVisible);
			if (!obbVisible)
				Check("OBB rejection has a vertex separating plane",
					HasSeparatingPlaneForVertices(frustum, tile));
		}
		constexpr int repetitions = 1000;
		const auto aabbStart = std::chrono::steady_clock::now();
		for (int repeat = 0; repeat < repetitions; ++repeat)
			for (const Tile& tile : tiles)
				benchmarkSink = benchmarkSink + frustum.IsBoxInFrustum(tile.aabb);
		const auto obbStart = std::chrono::steady_clock::now();
		for (int repeat = 0; repeat < repetitions; ++repeat)
			for (const Tile& tile : tiles)
				benchmarkSink = benchmarkSink + frustum.IsOBBInFrustum(tile.obb);
		const auto end = std::chrono::steady_clock::now();
		const double aabbMicroseconds = std::chrono::duration<double, std::micro>(obbStart - aabbStart).count() / repetitions;
		const double obbMicroseconds = std::chrono::duration<double, std::micro>(end - obbStart).count() / repetitions;
		std::printf("%-16s %12d %12d %12.2f %12.2f\n", camera.name,
			aabbKeep, obbKeep, aabbMicroseconds, obbMicroseconds);
	}
	std::printf("%llu mesh vertices and %llu random frustum/tile pairs checked; benchmark checksum %llu\n",
		static_cast<unsigned long long>(verticesChecked),
		static_cast<unsigned long long>(randomFrustumChecks),
		static_cast<unsigned long long>(benchmarkSink));
	return failures ? 1 : 0;
}
