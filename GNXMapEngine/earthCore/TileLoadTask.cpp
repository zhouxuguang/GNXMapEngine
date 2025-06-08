#include "TileLoadTask.h"
#include "ImageCodec/ColorConverter.h"
#include "RenderSystem/ImageTextureUtil.h"
#include "AssetProcess/DXTCompressor.h"
#include "TiledImage.h"

EARTH_CORE_NAMESPACE_BEGIN

static Texture2DPtr TextureFromImage(const imagecodec::VImage& image)
{
	/*uint8_t* pDest = new uint8_t[image.GetImageSize()];
	AssetProcess::CompressDXT1(pDest, image.GetPixels(), image.GetWidth(), image.GetHeight(), image.GetBytesPerRow());
	FILE* fp = fopen("1.dds", "wb");
	fwrite(pDest, 1, image.GetImageSize(), fp);
	fclose(fp);
	delete[] pDest;*/

	

	/*TextureDescriptor textureDescriptor = RenderSystem::ImageTextureUtil::getTextureDescriptor(image);
	textureDescriptor.mipmaped = true;

	Texture2DPtr texture = getRenderDevice()->createTextureWithDescriptor(textureDescriptor);
	Rect2D rect(0, 0, image.GetWidth(), image.GetHeight());
	texture->replaceRegion(rect, dxt1Image->GetPixels());
	return texture;*/

	if (image.GetFormat() == imagecodec::FORMAT_SRGB8)
	{
		imagecodec::VImagePtr dstImage = std::make_shared<imagecodec::VImage>();
		dstImage->SetImageInfo(imagecodec::FORMAT_RGBA8, image.GetWidth(), image.GetHeight());
		dstImage->AllocPixels();
		imagecodec::ColorConverter::convert_RGB24toRGBA32(image.GetPixels(), image.GetWidth() * image.GetHeight(), dstImage->GetPixels());

		VImagePtr dxt1Image = std::make_shared<VImage>();
		dxt1Image->SetImageInfo(FORMAT_DXT1_RGB, dstImage->GetWidth(), dstImage->GetHeight());
		dxt1Image->AllocPixels();
		AssetProcess::CompressDXT1(dxt1Image->GetPixels(), dstImage->GetPixels(), dstImage->GetWidth(), dstImage->GetHeight(), dstImage->GetBytesPerRow());
		
		TextureDescriptor textureDescriptor = RenderSystem::ImageTextureUtil::getTextureDescriptor(*dxt1Image);
		textureDescriptor.mipmaped = true;

		Texture2DPtr texture = getRenderDevice()->createTextureWithDescriptor(textureDescriptor);
		Rect2D rect(0, 0, image.GetWidth(), image.GetHeight());
		texture->replaceRegion(rect, dxt1Image->GetPixels());

		return texture;
	}

	TextureDescriptor textureDescriptor = RenderSystem::ImageTextureUtil::getTextureDescriptor(image);
	textureDescriptor.mipmaped = true;

	Texture2DPtr texture = getRenderDevice()->createTextureWithDescriptor(textureDescriptor);
	Rect2D rect(0, 0, image.GetWidth(), image.GetHeight());
	texture->replaceRegion(rect, image.GetPixels());
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
