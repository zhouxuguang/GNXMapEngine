#include "TileLoadTask.h"
#include "ImageCodec/ColorConverter.h"
#include "RenderSystem/ImageTextureUtil.h"
#include "AssetProcess/DXTCompressor.h"
#include "BaseLib/LogService.h"
#include "TiledImage.h"

EARTH_CORE_NAMESPACE_BEGIN

static Texture2DPtr TextureFromImage(const imagecodec::VImage& image)
{
	VImagePtr dxt1Image = std::make_shared<VImage>();
	dxt1Image->SetImageInfo(FORMAT_DXT1_RGB, image.GetWidth(), image.GetHeight());
	dxt1Image->AllocPixels();

	baselib::TimeCost cost;
	cost.Begin();
	for (int i = 0; i < 1000; i ++)
	{
		AssetProcess::CompressDXT1(dxt1Image->GetPixels(), image.GetPixels(), image.GetWidth(), image.GetHeight(), image.GetBytesPerRow());
	}
	cost.End();
	uint64_t t1 = cost.GetCostTimeNano();
	LOG_INFO("CompressDXT1 cost %lf", (double)t1);

	cost.Begin();
	for (int i = 0; i < 1000; i++)
	{
		AssetProcess::CompressDXT1_ISPC(dxt1Image->GetPixels(), image.GetPixels(), image.GetWidth(), image.GetHeight(), image.GetBytesPerRow());
	}
	cost.End();
	t1 = cost.GetCostTimeNano();
	LOG_INFO("CompressDXT1_ISPC cost %lf", (double)t1);

	TextureDescriptor textureDescriptor = RenderSystem::ImageTextureUtil::getTextureDescriptor(*dxt1Image);
	textureDescriptor.mipmaped = true;

	Texture2DPtr texture = getRenderDevice()->createTextureWithDescriptor(textureDescriptor);
	Rect2D rect(0, 0, image.GetWidth(), image.GetHeight());
	texture->replaceRegion(rect, dxt1Image->GetPixels());
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
