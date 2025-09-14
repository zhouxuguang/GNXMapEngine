#include "TileDataSource.h"
#include "TiledImage.h"

#if OS_MACOS
#include <sys/stat.h>
#endif

EARTH_CORE_NAMESPACE_BEGIN

long long get_file_size_stat(const char* filename) 
{
	struct stat file_stat;

	if (stat(filename, &file_stat) != 0) 
	{
		return -1; // Error opening file
	}

	return (long long)file_stat.st_size;
}

void verticalFlip(unsigned char* image, int width, int height, int channels) 
{
	int rowSize = width * channels;
	unsigned char* tempRow = new unsigned char[rowSize];

	for (int y = 0; y < height / 2; y++) 
	{
		unsigned char* top = image + y * rowSize;
		unsigned char* bottom = image + (height - 1 - y) * rowSize;

		// 交换行数据
		memcpy(tempRow, top, rowSize);
		memcpy(top, bottom, rowSize);
		memcpy(bottom, tempRow, rowSize);
	}

	delete[] tempRow;
}

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
    char szPathName[MAX_PATH_LENGHT] = {0};
	snprintf(szPathName, MAX_PATH_LENGHT
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
		snprintf(szPathName, MAX_PATH_LENGHT
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
			snprintf(szPathName, MAX_PATH_LENGHT
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

#if 0
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
#else
			long long fileSize = get_file_size_stat(szPathName);
			uint8_t* compressedData = new uint8_t[fileSize];
			fread(compressedData, 1, fileSize, pFile);
			fclose(pFile);
			size_t uncompressedDataSize = baselib::UnCompressBound(compressedData, fileSize, baselib::COMPRESS_GZIP);
			uint8_t* uncompressedData = new uint8_t[uncompressedDataSize];
			//baselib::DataUnCompress(compressedData, fileSize, uncompressedData, &uncompressedDataSize, baselib::COMPRESS_GZIP);

			float* pDst = image->heightData;
			int16_t* pDem = (int16_t*)compressedData;
			for (size_t i = 0; i < 65 * 65; i++)
			{
				pDst[i] = (float(pDem[i]) * 0.2 - 1000);
			}

			// 注意高度图是从上往下排列，这里将高度图反转
			verticalFlip((uint8_t*)pDst, 65, 65, 4);

			delete[] compressedData;
			delete[] uncompressedData;

			return image;
#endif
		}
	}

	return nullptr;
}

EARTH_CORE_NAMESPACE_END
