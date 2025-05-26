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
			sprintf(szPathName
				, "%s/%d/%d/%d.%s"
				, mDataPath.c_str()
				, int(tileID.level)
				, int(tileID.x)
				, int(tileID.y)
				, mExtName.c_str());

			FILE* pFile = fopen(szPathName, "rb");
			if (pFile == nullptr)
			{
				return  nullptr;
			}
			char szHeader[16] = { 0 };
			fread(szHeader, 1, sizeof(szHeader), pFile);
			if (strcmp(szHeader, "short") == 0)
			{
				short data[65 * 65] = {0};
				fread(data, 1, sizeof(data), pFile);
				fclose(pFile);
				
				float* pDst = image->heightData;
				for (size_t i = 0; i < 65 * 65; i++)
				{
					pDst[i] = data[i];
				}
				return image;
			}
			else if (strcmp(szHeader, "float") == 0)
			{
				float* pDst = image->heightData;
				fread(pDst, 1, 65 * 65 * 4, pFile);
				fclose(pFile);
				return image;
			}
			return nullptr;
		}
	}

	return nullptr;
}

EARTH_CORE_NAMESPACE_END