#include "TileDataSource.h"
#include "TiledImage.h"

EARTH_CORE_NAMESPACE_BEGIN

TileDataSource::TileDataSource(const std::string& dataPath, const std::string& extName)
{
    mDataPath = dataPath;
    mExtName = extName;
}

TileDataSource::~TileDataSource()
{
}

ObjectBasePtr TileDataSource::ReadTile(const QuadTileID& tileID)
{
	const uint32_t MAX_PATH_LENGHT = 512;
	char szPathName[MAX_PATH_LENGHT];
	sprintf(szPathName
		, "%s/%d/%d/%d.%s"
		, mDataPath.c_str()
		, int(tileID.level)
		, int(tileID.x)
		, int(tileID.y)
		, mExtName.c_str());

	TiledImagePtr image = std::make_shared<TiledImage>();
	if (!image)
	{
		return nullptr;
	}

	bool ret = imagecodec::ImageDecoder::DecodeFile(szPathName, &image->image);
	if (ret)
	{
		return image;
	}
	else
	{
		sprintf(szPathName
			, "%s/%d/%d/%d..%s"
			, mDataPath.c_str()
			, int(tileID.level)
			, int(tileID.x)
			, int(tileID.y)
			, mExtName.c_str());

		ret = imagecodec::ImageDecoder::DecodeFile(szPathName, &image->image);
		if (ret)
		{
			return image;
		}
		else
		{
			return nullptr;
		}
	}

	return nullptr;
}

EARTH_CORE_NAMESPACE_END