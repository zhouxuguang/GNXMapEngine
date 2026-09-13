#include "TileLoadTask.h"
#include "Runtime/ImageCodec/include/ColorConverter.h"
#include "Runtime/RenderSystem/include/ImageTextureUtil.h"
#include "Runtime/BaseLib/include/LogService.h"
#include "TiledImage.h"

EARTH_CORE_NAMESPACE_BEGIN

static RCTexturePtr TextureFromImage(const imagecodec::VImage& image)
{
	TextureDesc textureDescriptor = RenderSystem::ImageTextureUtil::getTextureDescriptor(image);

    RCTexture2DPtr texture = GetRenderDevice()->CreateTexture2D(textureDescriptor.format,
                                                              TextureUsage::TextureUsageShaderRead,
                                                              textureDescriptor.width, textureDescriptor.height, 1);
	Rect2D rect(0, 0, image.GetWidth(), image.GetHeight());
    texture->ReplaceRegion(rect, 0, image.GetImageData(), image.GetBytesPerRow());
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
