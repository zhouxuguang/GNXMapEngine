#include "TileLoadTask.h"
#include "Runtime/ImageCodec/include/ColorConverter.h"
#include "Runtime/RenderSystem/include/ImageTextureUtil.h"
#include "Runtime/BaseLib/include/LogService.h"
#include "TiledImage.h"
#include <vector>

EARTH_CORE_NAMESPACE_BEGIN

static RCTexturePtr TextureFromImage(const imagecodec::VImage& image)
{
	TextureDesc textureDescriptor = RenderSystem::ImageTextureUtil::getTextureDescriptor(image);

	const uint32_t width = image.GetWidth();
	const uint32_t height = image.GetHeight();
	const uint8_t* data = image.GetImageData();
	uint32_t bytesPerRow = image.GetBytesPerRow();

	// Vulkan 后端不支持 24 位 RGB 采样格式（kTexFormatRGB24 无对应 VkFormat，
	// 创建纹理会得到 VK_FORMAT_UNDEFINED 并导致失败）。这里与
	// ImageTextureUtil::TextureFromFile 保持一致，先把 RGB8/SRGB8 转成 32 位格式。
	std::vector<uint8_t> converted;
	const imagecodec::ImagePixelFormat format = image.GetFormat();
	if (format == imagecodec::FORMAT_RGB8 || format == imagecodec::FORMAT_SRGB8)
	{
		const bool isSRGB = (format == imagecodec::FORMAT_SRGB8);
		const uint32_t dstBytesPerRow = width * 4;
		converted.resize((size_t)dstBytesPerRow * height);

		for (uint32_t y = 0; y < height; ++y)
		{
			const uint8_t* srcRow = data + (size_t)y * bytesPerRow;
			uint8_t* dstRow = converted.data() + (size_t)y * dstBytesPerRow;
			for (uint32_t x = 0; x < width; ++x)
			{
				dstRow[x * 4 + 0] = srcRow[x * 3 + 0];
				dstRow[x * 4 + 1] = srcRow[x * 3 + 1];
				dstRow[x * 4 + 2] = srcRow[x * 3 + 2];
				dstRow[x * 4 + 3] = 255;
			}
		}

		data = converted.data();
		bytesPerRow = dstBytesPerRow;
		textureDescriptor.format = isSRGB ? RenderCore::kTexFormatSRGB8_ALPHA8 : RenderCore::kTexFormatRGBA8;
	}

	if (width == 0 || height == 0 || data == nullptr ||
		textureDescriptor.format == RenderCore::kTexFormatInvalid)
	{
		LOG_ERROR("TextureFromImage: invalid image (size=%ux%u, format=%d)", width, height, (int)format);
		return nullptr;
	}

    RCTexture2DPtr texture = GetRenderDevice()->CreateTexture2D(textureDescriptor.format,
                                                              TextureUsage::TextureUsageShaderRead,
                                                              width, height, 1);
	if (!texture)
	{
		LOG_ERROR("TextureFromImage: create texture failed (size=%ux%u, format=%u)", width, height, textureDescriptor.format);
		return nullptr;
	}

	Rect2D rect(0, 0, width, height);
    texture->ReplaceRegion(rect, 0, data, bytesPerRow);
	return texture;
}

TileLoadTask::TileLoadTask()
{
}

TileLoadTask::~TileLoadTask()
{
}

// 这里是实际加载数据的逻辑
void TileLoadTask::Run()
{
    ObjectBasePtr tileData = layer->ReadTile(tileId);
    if (tileData)
    {
		if (layer->GetLayerType() == LayerType::LT_Image)
		{
			// 节点加上有影像的标记
			nodePtr->mStatusFlag |= FLAG_HAS_IMAGE;
			// 节点加上可以渲染的标记
			nodePtr->mStatusFlag |= FLAG_RENDER;

			nodePtr->mTexture = TextureFromImage(tileData->toPtr<TiledImage>()->image);
		}

		else if (layer->GetLayerType() == LayerType::LT_Terrain)
		{
			// 节点加上有影像的标记
			nodePtr->mStatusFlag |= FLAG_HAS_IMAGE;
			// 节点加上可以渲染的标记
			nodePtr->mStatusFlag |= FLAG_RENDER;

			nodePtr->mDemData.FillHeight(tileData->toPtr<TiledImage>()->heightData);
			nodePtr->mDemData.FillVertex();
		}
        
    }
}

EARTH_CORE_NAMESPACE_END
