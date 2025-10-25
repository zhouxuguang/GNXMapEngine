/**
 * 
 */

#ifndef GNXMAP_ENGINE_GLOBAL_RECTANGLE_INCLUDE_GDJGJ
#define GNXMAP_ENGINE_GLOBAL_RECTANGLE_INCLUDE_GDJGJ

#include "EarthEngineDefine.h"
#include "Geodetic3D.h"

EARTH_CORE_NAMESPACE_BEGIN

/**
 * @brief 在球面上是矩形，在直角坐标系统中就可能不是
 */
class GlobeRectangle 
{
public:
    /**
    * @brief An empty rectangle.
    *
    * The rectangle has the following values:
    *   * `west`: Pi
    *   * `south`: Pi/2
    *   * `east`: -Pi
    *   * `north`: -Pi/2
    */
    static const GlobeRectangle EMPTY;

    /**
    * @brief The maximum rectangle.
    *
    * The rectangle has the following values:
    *   * `west`: -Pi
    *   * `south`: -Pi/2
    *   * `east`: Pi
    *   * `north`: Pi/2
    */
    static const GlobeRectangle MAXIMUM;

    /**
    * @brief Constructs a new instance.
    *
    * @param west The westernmost longitude, in radians, in the range [-Pi, Pi].
    * @param south The southernmost latitude, in radians, in the range [-Pi/2,
    * Pi/2].
    * @param east The easternmost longitude, in radians, in the range [-Pi, Pi].
    * @param north The northernmost latitude, in radians, in the range [-Pi/2,
    * Pi/2].
    */
    constexpr GlobeRectangle(
        double west,
        double south,
        double east,
        double north) noexcept
        : mWest(west), mSouth(south), mEast(east), mNorth(north) {}

    /**
    * Creates a rectangle given the boundary longitude and latitude in degrees.
    * The angles are converted to radians.
    *
    * @param westDegrees The westernmost longitude in degrees in the range
    * [-180.0, 180.0].
    * @param southDegrees The southernmost latitude in degrees in the range
    * [-90.0, 90.0].
    * @param eastDegrees The easternmost longitude in degrees in the range
    * [-180.0, 180.0].
    * @param northDegrees The northernmost latitude in degrees in the range
    * [-90.0, 90.0].
    * @returns The rectangle.
    *
    * @snippet TestGlobeRectangle.cpp fromDegrees
    */
    static constexpr GlobeRectangle fromDegrees(
        double westDegrees,
        double southDegrees,
        double eastDegrees,
        double northDegrees) noexcept 
    {
		return GlobeRectangle(
			::degToRad(westDegrees),
			::degToRad(southDegrees),
			::degToRad(eastDegrees),
			::degToRad(northDegrees));
    }

    /**
    * @brief Returns the westernmost longitude, in radians.
    */
    constexpr double getWest() const noexcept { return this->mWest; }

    /**
    * @brief Returns the southernmost latitude, in radians.
    */
    constexpr double getSouth() const noexcept { return this->mSouth; }

    /**
    * @brief Returns the easternmost longitude, in radians.
    */
    constexpr double getEast() const noexcept { return this->mEast; }

    /**
    * @brief Returns the northernmost latitude, in radians.
    */
    constexpr double getNorth() const noexcept { return this->mNorth; }

    /**
	* @brief Computes the width of this rectangle.
	*
	* The result will be in radians, in the range [0, Pi*2].
	*/
    constexpr double computeWidth() const noexcept 
    {
        double east = this->mEast;
        const double west = this->mWest;
        if (east < west) 
        {
            east += M_PI * 2.0;
        }
        return east - west;
    }

    /**
     * @brief Computes the height of this rectangle.
     *
     * The result will be in radians, in the range [0, Pi*2].
     */
    constexpr double computeHeight() const noexcept 
    {
        return this->mNorth - this->mSouth;
    }

    Geodetic3D computeCenter() const noexcept;

private:
	double mWest;
	double mSouth;
	double mEast;
	double mNorth;
};

EARTH_CORE_NAMESPACE_END

#endif

