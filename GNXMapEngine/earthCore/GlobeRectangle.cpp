
#include "GlobeRectangle.h"

EARTH_CORE_NAMESPACE_BEGIN

const GlobeRectangle GlobeRectangle::EMPTY
{
    M_PI,
    M_PI_2,
    -M_PI,
    -M_PI_2
};

const GlobeRectangle GlobeRectangle::MAXIMUM
{
	-M_PI,
	-M_PI_2,
	M_PI,
	M_PI_2
};

Geodetic3D GlobeRectangle::computeCenter() const noexcept
{
	double latitudeCenter = (this->mSouth + this->mNorth) * 0.5;

	if (this->mWest <= this->mEast) 
	{
		// 简单矩形，没有跨越国际日期变更线
		return Geodetic3D((this->mWest + this->mEast) * 0.5, latitudeCenter, 0.0);
	}
	else 
	{
		// 跨越了国际日期变更线
		double westToAntiMeridian = M_PI - this->mWest;
		double antiMeridianToEast = this->mEast - -M_PI;
		double total = westToAntiMeridian + antiMeridianToEast;
		if (westToAntiMeridian >= antiMeridianToEast) 
		{
			// Center is in the Eastern hemisphere.
			return Geodetic3D(
				std::min(M_PI, this->mEast + total * 0.5),
				latitudeCenter,
				0.0);
		}
		else 
		{
			// Center is in the Western hemisphere.
			return Geodetic3D(
				std::max(-M_PI, this->mEast - total * 0.5),
				latitudeCenter,
				0.0);
		}
	}
}

EARTH_CORE_NAMESPACE_END
