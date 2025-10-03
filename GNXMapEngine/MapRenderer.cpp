//
//  MapRenderer.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#include "MapRenderer.h"
#include <QtCore>
#include "RenderSystem/SceneManager.h"
#include "RenderSystem/SceneNode.h"
#include "RenderSystem/mesh/MeshRenderer.h"
#include "RenderSystem/ArcballManipulate.h"
#include "MathUtil/Vector3.h"
#include "RenderSystem/SkyBoxNode.h"
#include "ImageCodec/ImageDecoder.h"
#include "RenderSystem/RenderEngine.h"
#include "RenderSystem/ImageTextureUtil.h"
#include "BaseLib/DateTime.h"
#include "BaseLib/LogService.h"
#include "ImageCodec/ColorConverter.h"
#include "RenderSystem/Atmosphere/AtmosphereModel.h"
#include "RenderSystem/Atmosphere/AtmosphereRenderer.h"

#include "WebMercator.h"
//#include "httplib.h"
#include "earthCore/Ellipsoid.h"
#include "earthCore/GeoGridTessellator.h"
#include "earthCore/EarthNode.h"
#include "earthCore/EarthCamera.h"
#include "earthCore/QuadTree.h"
#include "earthCore/LayerBase.h"
#include "earthCore/EarthRenderer.h"

#include <filesystem>

namespace fs = std::filesystem;

using namespace RenderCore;
using namespace RenderSystem;

MapRenderer::MapRenderer(void *metalLayer)
{
#if OS_WINDOWS
    mRenderdevice = CreateRenderDevice(RenderDeviceType::VULKAN, metalLayer);
#elif OS_MACOS
    //mRenderdevice = CreateRenderDevice(RenderDeviceType::VULKAN, metalLayer);
    mRenderdevice = CreateRenderDevice(RenderDeviceType::METAL, metalLayer);
#endif // _WIN

    
    mSceneManager = SceneManager::GetInstance();
    
    BuildEarthNode();
}

void MapRenderer::SetWindowSize(uint32_t width, uint32_t height)
{
    mWidth = width;
    mHeight = height;
    
    mRenderdevice->Resize(width, height);
    
    mSceneManager->AddCamara(mCameraPtr);
    mCameraPtr->SetLens(60, float(width) / height, 10, 6378137.0 * 4);
    mCameraPtr->SetViewSize(width, height);
    
//    cameraPtr->LookAt(Vector3f(2, 0, 0), Vector3f(0, 0, 0), Vector3f(0, 0, 1));
//    cameraPtr->SetLens(60, float(width) / height, 0.1f, 100);
    
    //初始化灯光信息
    Light * pointLight = mSceneManager->createLight("mainLight", Light::LightType::PointLight);
    pointLight->setColor(Vector3f(1.0, 1.0, 1.0));
    //pointLight->setPosition(Vector3f(5.0, 8.0, 0.0));
    pointLight->setPosition(Vector3f(-1.0, -1.0, -1.0));
    pointLight->setFalloffStart(5);
    pointLight->setFalloffEnd(300);
    pointLight->setStrength(Vector3f(8.0, 8.0, 8.0));
}

void MapRenderer::Zoom(double deltaDistance)
{
    mCameraPtr->Zoom(deltaDistance);
}

void MapRenderer::Pan(float offsetX, float offsetY)
{
    mCameraPtr->Pan(offsetX, offsetY);
}

static PostProcessing* postProcessing = nullptr;

void initPostResource(RenderDevicePtr renderDevice)
{
    postProcessing = new PostProcessing(renderDevice);
}

void testPost(const RenderEncoderPtr &renderEncoder, const RCTexturePtr texture)
{
    postProcessing->SetRenderTexture(texture);
    postProcessing->Process(renderEncoder);
}

struct ScatteringCB
{
	int layer;  // 当前散射层
	int scattering_order;
};

void MapRenderer::TestAtmo()
{
    InitAtmo();
    
    CommandBufferPtr commandBuffer = mRenderdevice->CreateCommandBuffer();
        
    // 
    {
		RenderPass renderPass;
		RenderPassColorAttachmentPtr colorAttachmentPtr = std::make_shared<RenderPassColorAttachment>();
		colorAttachmentPtr->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
		colorAttachmentPtr->texture = transmittance_texture;
		renderPass.colorAttachments.push_back(colorAttachmentPtr);

		renderPass.renderRegion = Rect2D(0, 0, Atmosphere::TRANSMITTANCE_TEXTURE_WIDTH, Atmosphere::TRANSMITTANCE_TEXTURE_HEIGHT);
		RenderEncoderPtr renderEncoder1 = commandBuffer->CreateRenderEncoder(renderPass);

		renderEncoder1->SetGraphicsPipeline(mPipeline1);
		renderEncoder1->SetFragmentUniformBuffer("AtmosphereParametersCB", mUBO);
		renderEncoder1->DrawPrimitves(PrimitiveMode_TRIANGLES, 0, 3);

		renderEncoder1->EndEncode();
    }

    // 计算直接辐照度，存储到delta_irradiance_texture中 irradiance_texture_存储地面接收的天空辐照度
    {
		RenderPass renderPass;
		RenderPassColorAttachmentPtr colorAttachmentPtr = std::make_shared<RenderPassColorAttachment>();
		colorAttachmentPtr->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
		colorAttachmentPtr->texture = delta_irradiance_texture;
		renderPass.colorAttachments.push_back(colorAttachmentPtr);

		renderPass.renderRegion = Rect2D(0, 0, Atmosphere::IRRADIANCE_TEXTURE_WIDTH, Atmosphere::IRRADIANCE_TEXTURE_HEIGHT);
		RenderEncoderPtr renderEncoder1 = commandBuffer->CreateRenderEncoder(renderPass);

		renderEncoder1->SetGraphicsPipeline(mPipeline2);
		renderEncoder1->SetFragmentUniformBuffer("AtmosphereParametersCB", mUBO);
        renderEncoder1->SetFragmentTextureAndSampler("transmittance_texture", transmittance_texture, sampler);
		renderEncoder1->DrawPrimitves(PrimitiveMode_TRIANGLES, 0, 3);

		renderEncoder1->EndEncode();
    }

    {
		for (uint32_t i = 0; i < Atmosphere::SCATTERING_TEXTURE_DEPTH; i++)
		{
            RenderPass renderPass;
            RenderPassColorAttachmentPtr colorAttachmentPtr1 = std::make_shared<RenderPassColorAttachment>();
            colorAttachmentPtr1->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
            colorAttachmentPtr1->texture = delta_rayleigh_scattering_texture;
            colorAttachmentPtr1->slice = i;
            renderPass.colorAttachments.push_back(colorAttachmentPtr1);
            
            RenderPassColorAttachmentPtr colorAttachmentPtr2 = std::make_shared<RenderPassColorAttachment>();
            colorAttachmentPtr2->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
            colorAttachmentPtr2->texture = delta_mie_scattering_texture;
            colorAttachmentPtr2->slice = i;
            renderPass.colorAttachments.push_back(colorAttachmentPtr2);
            
            RenderPassColorAttachmentPtr colorAttachmentPtr3 = std::make_shared<RenderPassColorAttachment>();
            colorAttachmentPtr3->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
            colorAttachmentPtr3->texture = scattering_texture;
            colorAttachmentPtr3->slice = i;
            renderPass.colorAttachments.push_back(colorAttachmentPtr3);
            
            RenderPassColorAttachmentPtr colorAttachmentPtr4 = std::make_shared<RenderPassColorAttachment>();
            colorAttachmentPtr4->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
            colorAttachmentPtr4->texture = optional_single_mie_scattering_texture;
            colorAttachmentPtr4->slice = i;
            renderPass.colorAttachments.push_back(colorAttachmentPtr4);

            renderPass.renderRegion = Rect2D(0, 0, Atmosphere::SCATTERING_TEXTURE_WIDTH, Atmosphere::SCATTERING_TEXTURE_HEIGHT);
            renderPass.layerCount = Atmosphere::SCATTERING_TEXTURE_DEPTH;
            RenderEncoderPtr renderEncoder1 = commandBuffer->CreateRenderEncoder(renderPass);

            renderEncoder1->SetGraphicsPipeline(mPipeline3);
            renderEncoder1->SetFragmentUniformBuffer("AtmosphereParametersCB", mUBO);
            renderEncoder1->SetFragmentTextureAndSampler("transmittance_texture", transmittance_texture, sampler);
            
            ScatteringCB scatteringCB = {};
            scatteringCB.layer = i;
            mUBOs[i]->SetData(&scatteringCB, 0, sizeof(scatteringCB));
            renderEncoder1->SetFragmentUniformBuffer("ScatteringCB", mUBOs[i]);
            
            renderEncoder1->DrawPrimitves(PrimitiveMode_TRIANGLES, 0, 3);

            renderEncoder1->EndEncode();
		}
        
        /**
         * @brief 计算多重散射
         delta_multiple_scattering_texture用于计算3次及3次以上的多重散射
         而delta_rayleigh_scattering_texture仅用于双重散射,为了节省内存，共用同一个纹理
         * 
         */
        for (unsigned int scattering_order = 2; scattering_order <= 2; ++scattering_order)
        {
            // 计算特定(r,mu)接收的辐照度，存储到delta_scattering_density_texture中
            for (int layer = 0; layer < Atmosphere::SCATTERING_TEXTURE_DEPTH; ++layer)
            {
                RenderPass renderPass;
                RenderPassColorAttachmentPtr colorAttachmentPtr = std::make_shared<RenderPassColorAttachment>();
                colorAttachmentPtr->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
                colorAttachmentPtr->texture = delta_scattering_density_texture;
                colorAttachmentPtr->slice = layer;
                renderPass.colorAttachments.push_back(colorAttachmentPtr);
                
                renderPass.renderRegion = Rect2D(0, 0, Atmosphere::SCATTERING_TEXTURE_WIDTH, Atmosphere::SCATTERING_TEXTURE_HEIGHT);
                renderPass.layerCount = Atmosphere::SCATTERING_TEXTURE_DEPTH;
                RenderEncoderPtr renderEncoder1 = commandBuffer->CreateRenderEncoder(renderPass);
                renderEncoder1->SetGraphicsPipeline(mPipeline4);
                
                renderEncoder1->SetFragmentUniformBuffer("AtmosphereParametersCB", mUBO);
                
                ScatteringCB scatteringCB = {};
                scatteringCB.layer = layer;
                scatteringCB.scattering_order = scattering_order;
                mUBOs[layer]->SetData(&scatteringCB, 0, sizeof(scatteringCB));
                renderEncoder1->SetFragmentUniformBuffer("ScatteringCB", mUBOs[layer]);
                
                renderEncoder1->SetFragmentTextureAndSampler("transmittance_texture", transmittance_texture, nullptr);
                renderEncoder1->SetFragmentTextureAndSampler("single_rayleigh_scattering_texture", delta_rayleigh_scattering_texture, nullptr);
                renderEncoder1->SetFragmentTextureAndSampler("single_mie_scattering_texture", delta_mie_scattering_texture, nullptr);
                renderEncoder1->SetFragmentTextureAndSampler("multiple_scattering_texture", delta_rayleigh_scattering_texture, nullptr);
                renderEncoder1->SetFragmentTextureAndSampler("irradiance_texture", delta_irradiance_texture, nullptr);
                
                renderEncoder1->DrawPrimitves(PrimitiveMode_TRIANGLES, 0, 3);

                renderEncoder1->EndEncode();
            }
            
            // 计算间接辐照度，存储到delta_irradiance_texture中, 然后累加到irradiance_texture_
            {
                RenderPass renderPass;
                RenderPassColorAttachmentPtr colorAttachmentPtr1 = std::make_shared<RenderPassColorAttachment>();
                colorAttachmentPtr1->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
                colorAttachmentPtr1->texture = delta_irradiance_texture;
                colorAttachmentPtr1->loadOp = ATTACHMENT_LOAD_OP_LOAD;
                colorAttachmentPtr1->storeOp = ATTACHMENT_STORE_OP_STORE;
                renderPass.colorAttachments.push_back(colorAttachmentPtr1);
                
                RenderPassColorAttachmentPtr colorAttachmentPtr2 = std::make_shared<RenderPassColorAttachment>();
                colorAttachmentPtr2->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
                colorAttachmentPtr2->texture = irradiance_texture;
                colorAttachmentPtr2->loadOp = ATTACHMENT_LOAD_OP_LOAD;
                colorAttachmentPtr2->storeOp = ATTACHMENT_STORE_OP_STORE;
                renderPass.colorAttachments.push_back(colorAttachmentPtr2);
                
                renderPass.renderRegion = Rect2D(0, 0, Atmosphere::IRRADIANCE_TEXTURE_WIDTH, Atmosphere::IRRADIANCE_TEXTURE_HEIGHT);
                RenderEncoderPtr renderEncoder1 = commandBuffer->CreateRenderEncoder(renderPass);
                renderEncoder1->SetGraphicsPipeline(mPipeline5);
                
                renderEncoder1->SetFragmentUniformBuffer("AtmosphereParametersCB", mUBO);
                
                ScatteringCB scatteringCB = {};
                scatteringCB.scattering_order = scattering_order - 1;
                mUBOs[0]->SetData(&scatteringCB, 0, sizeof(scatteringCB));
                renderEncoder1->SetFragmentUniformBuffer("ScatteringCB", mUBOs[0]);
                
                renderEncoder1->SetFragmentTextureAndSampler("single_rayleigh_scattering_texture", delta_rayleigh_scattering_texture, nullptr);
                renderEncoder1->SetFragmentTextureAndSampler("single_mie_scattering_texture", delta_mie_scattering_texture, nullptr);
                renderEncoder1->SetFragmentTextureAndSampler("multiple_scattering_texture", delta_rayleigh_scattering_texture, nullptr);
                
                renderEncoder1->DrawPrimitves(PrimitiveMode_TRIANGLES, 0, 3);

                renderEncoder1->EndEncode();
            }
        }
    }
    
    RenderEncoderPtr renderEncoder = commandBuffer->CreateDefaultRenderEncoder();
    testPost(renderEncoder, delta_irradiance_texture);
    renderEncoder->EndEncode();
    commandBuffer->PresentFrameBuffer();
}

void MapRenderer::InitAtmo()
{
    if (!postProcessing)
    {
        initPostResource(mRenderdevice);
    }

    if (!sampler)
    {
        SamplerDescriptor des;
        sampler = mRenderdevice->CreateSamplerWithDescriptor(des);
    }

	if (!transmittance_texture)
	{
		transmittance_texture = mRenderdevice->CreateTexture2D(kTexFormatRGBA32Float,
                                                               TextureUsage::TextureUsageRenderTarget,
                                                               Atmosphere::TRANSMITTANCE_TEXTURE_WIDTH,
                                                               Atmosphere::TRANSMITTANCE_TEXTURE_HEIGHT, 1);
	}
	if (!mPipeline1)
	{
		ShaderAssetString shaderAssetString = LoadShaderAsset("Atmosphere/ComputeTransmittance");

		ShaderCodePtr vertexShader = shaderAssetString.vertexShader->shaderSource;
		ShaderCodePtr fragmentShader = shaderAssetString.fragmentShader->shaderSource;

		GraphicsShaderPtr graphicsShader = mRenderdevice->CreateGraphicsShader(*vertexShader, *fragmentShader);

		GraphicsPipelineDescriptor graphicsPipelineDescriptor;
		graphicsPipelineDescriptor.vertexDescriptor = shaderAssetString.vertexDescriptor;

		mPipeline1 = mRenderdevice->CreateGraphicsPipeline(graphicsPipelineDescriptor);
		mPipeline1->AttachGraphicsShader(graphicsShader);
	}

	if (!delta_irradiance_texture)
	{
		delta_irradiance_texture = mRenderdevice->CreateTexture2D(kTexFormatRGBA32Float,
                                                                  TextureUsage::TextureUsageRenderTarget,
                                                                  Atmosphere::IRRADIANCE_TEXTURE_WIDTH, 
                                                                  Atmosphere::IRRADIANCE_TEXTURE_HEIGHT, 1);
	}
	if (!mPipeline2)
	{
		ShaderAssetString shaderAssetString = LoadShaderAsset("Atmosphere/ComputeDirectIrradiance");

		ShaderCodePtr vertexShader = shaderAssetString.vertexShader->shaderSource;
		ShaderCodePtr fragmentShader = shaderAssetString.fragmentShader->shaderSource;

		GraphicsShaderPtr graphicsShader = mRenderdevice->CreateGraphicsShader(*vertexShader, *fragmentShader);

		GraphicsPipelineDescriptor graphicsPipelineDescriptor;
		graphicsPipelineDescriptor.vertexDescriptor = shaderAssetString.vertexDescriptor;

        mPipeline2 = mRenderdevice->CreateGraphicsPipeline(graphicsPipelineDescriptor);
        mPipeline2->AttachGraphicsShader(graphicsShader);
	}

	if (!mUBO)
	{
		size_t size = sizeof(Atmosphere::AtmosphereParameters);
		mUBO = mRenderdevice->CreateUniformBufferWithSize((uint32_t)size);
		AtmosphereModel* model = CreateAtmoModel();
		mUBO->SetData(&model->GetAtmosphereParameters(), 0, (uint32_t)size);
	}

    if (!mPipeline3)
    {
		delta_rayleigh_scattering_texture = mRenderdevice->CreateTexture3D(kTexFormatRGBA32,
			TextureUsage::TextureUsageRenderTarget,
			Atmosphere::SCATTERING_TEXTURE_WIDTH,
			Atmosphere::SCATTERING_TEXTURE_HEIGHT, 
            Atmosphere::SCATTERING_TEXTURE_DEPTH, 1);

        delta_mie_scattering_texture = mRenderdevice->CreateTexture3D(kTexFormatRGBA32,
			TextureUsage::TextureUsageRenderTarget,
			Atmosphere::SCATTERING_TEXTURE_WIDTH,
			Atmosphere::SCATTERING_TEXTURE_HEIGHT,
			Atmosphere::SCATTERING_TEXTURE_DEPTH, 1);

        scattering_texture = mRenderdevice->CreateTexture3D(kTexFormatRGBA32,
			TextureUsage::TextureUsageRenderTarget,
			Atmosphere::SCATTERING_TEXTURE_WIDTH,
			Atmosphere::SCATTERING_TEXTURE_HEIGHT,
			Atmosphere::SCATTERING_TEXTURE_DEPTH, 1);

        optional_single_mie_scattering_texture = mRenderdevice->CreateTexture3D(kTexFormatRGBA32,
			TextureUsage::TextureUsageRenderTarget,
			Atmosphere::SCATTERING_TEXTURE_WIDTH,
			Atmosphere::SCATTERING_TEXTURE_HEIGHT,
			Atmosphere::SCATTERING_TEXTURE_DEPTH, 1);

		ShaderAssetString shaderAssetString = LoadShaderAsset("Atmosphere/ComputeSingleScattering");

		ShaderCodePtr vertexShader = shaderAssetString.vertexShader->shaderSource;
		ShaderCodePtr fragmentShader = shaderAssetString.fragmentShader->shaderSource;

		GraphicsShaderPtr graphicsShader = mRenderdevice->CreateGraphicsShader(*vertexShader, *fragmentShader);

		GraphicsPipelineDescriptor graphicsPipelineDescriptor;
		graphicsPipelineDescriptor.vertexDescriptor = shaderAssetString.vertexDescriptor;

		mPipeline3 = mRenderdevice->CreateGraphicsPipeline(graphicsPipelineDescriptor);
        mPipeline3->AttachGraphicsShader(graphicsShader);

		size_t size = sizeof(ScatteringCB);
        ScatteringCB scatteringData = {};

        for (uint32_t i = 0; i < 256; i ++)
        {
            RenderCore::UniformBufferPtr ubo = mRenderdevice->CreateUniformBufferWithSize((uint32_t)size);
            mUBOs.push_back(ubo);
        }
    }
    
    if (!mPipeline4)
    {
        delta_scattering_density_texture = mRenderdevice->CreateTexture3D(kTexFormatRGBA32,
            TextureUsage::TextureUsageRenderTarget,
            Atmosphere::SCATTERING_TEXTURE_WIDTH,
            Atmosphere::SCATTERING_TEXTURE_HEIGHT,
            Atmosphere::SCATTERING_TEXTURE_DEPTH, 1);
        
        ShaderAssetString shaderAssetString = LoadShaderAsset("Atmosphere/ComputeScatteringDensity");

        ShaderCodePtr vertexShader = shaderAssetString.vertexShader->shaderSource;
        ShaderCodePtr fragmentShader = shaderAssetString.fragmentShader->shaderSource;

        GraphicsShaderPtr graphicsShader = mRenderdevice->CreateGraphicsShader(*vertexShader, *fragmentShader);

        GraphicsPipelineDescriptor graphicsPipelineDescriptor;
        graphicsPipelineDescriptor.vertexDescriptor = shaderAssetString.vertexDescriptor;

        mPipeline4 = mRenderdevice->CreateGraphicsPipeline(graphicsPipelineDescriptor);
        mPipeline4->AttachGraphicsShader(graphicsShader);
    }
    
    if (!mPipeline5)
    {
        ShaderAssetString shaderAssetString = LoadShaderAsset("Atmosphere/ComputeIndirectIrradiance");

        ShaderCodePtr vertexShader = shaderAssetString.vertexShader->shaderSource;
        ShaderCodePtr fragmentShader = shaderAssetString.fragmentShader->shaderSource;

        GraphicsShaderPtr graphicsShader = mRenderdevice->CreateGraphicsShader(*vertexShader, *fragmentShader);

        GraphicsPipelineDescriptor graphicsPipelineDescriptor;
        graphicsPipelineDescriptor.vertexDescriptor = shaderAssetString.vertexDescriptor;

        mPipeline5 = mRenderdevice->CreateGraphicsPipeline(graphicsPipelineDescriptor);
        mPipeline5->AttachGraphicsShader(graphicsShader);
        
        irradiance_texture = mRenderdevice->CreateTexture2D(kTexFormatRGBA32Float,
                                                            TextureUsage::TextureUsageRenderTarget,
                                                            Atmosphere::IRRADIANCE_TEXTURE_WIDTH,
                                                            Atmosphere::IRRADIANCE_TEXTURE_HEIGHT, 1);
    }
}

void MapRenderer::DrawFrame()
{
    if (!mRenderdevice)
    {
        return;
    }
    
    uint64_t thisTime = GetTickNanoSeconds();
    float deltaTime = float(thisTime - mLastTime) * 0.000000001f;
    LOG_INFO("deltaTime = %f", deltaTime);
    mLastTime = thisTime;
    
    mSceneManager->Update(deltaTime);
    
    TestAtmo();
    return;
    
    CommandBufferPtr commandBuffer = mRenderdevice->CreateCommandBuffer();
    if (!commandBuffer)
    {
        return;
    }
    RenderEncoderPtr renderEncoder = commandBuffer->CreateDefaultRenderEncoder();
    
    mSceneManager->Render(renderEncoder);
    
    renderEncoder->EndEncode();
    commandBuffer->PresentFrameBuffer();
}

void MapRenderer::BuildEarthNode()
{
    earthcore::Ellipsoid wgs84 = earthcore::Ellipsoid::WGS84;
    earthcore::Geodetic3D geodetic3D(0, 0, 0);
    Vector3d position = wgs84.CartographicToCartesian(geodetic3D);
    earthcore::Geodetic3D geodetic3D1 = wgs84.CartesianToCartographic(position);
    
    // 这句删掉为啥显示异常？
    MeshPtr mesh = earthcore::GeoGridTessellator::Compute(wgs84, 360, 180, earthcore::GeoGridTessellator::GeoGridVertexAttributes::All);
    MaterialPtr material = Material::GetDefaultDiffuseMaterial();

    // 创建相机
	mCameraPtr = std::make_shared<earthcore::EarthCamera>(wgs84, "MainCamera");
    earthcore::EarthNode *pEarthNode = new earthcore::EarthNode(wgs84, mCameraPtr);

    // 增加数据源
#if OS_MACOS
    fs::path dataPath = R"(/Users/zhouxuguang/work/data/gis/tile/image)";
    fs::path demPath = R"(/Users/zhouxuguang/work/data/gis/tile/terrain)";
#elif OS_WINDOWS
    fs::path dataPath = R"(D:/source/gis/data/tile/image)";
    //fs::path demPath = R"(D:/source/gis/data/tile/terrain)";
    fs::path demPath = R"(D:/source/gis/gdal/cesium-terrain-builder/build/Debug/terrain-tiles/test)";
#endif

    std::string curDir = baselib::EnvironmentUtility::GetInstance().GetCurrentWorkingDir();

    LOG_INFO("%s", curDir.c_str());
    earthcore::TileDataSourcePtr imageSource = std::make_shared<earthcore::TileDataSource>(dataPath.string(), "jpg");
    earthcore::LayerBasePtr imageLayer = std::make_shared<earthcore::LayerBase>("Image", earthcore::LT_Image);
    imageLayer->SetDataSource(imageSource);

	earthcore::TileDataSourcePtr demSource = std::make_shared<earthcore::TileDataSource>(demPath.string(), "terrain");
	earthcore::LayerBasePtr demLayer = std::make_shared<earthcore::LayerBase>("DEM", earthcore::LT_Terrain);
    demLayer->SetDataSource(demSource);
    
    pEarthNode->AddLayer(imageLayer);
    pEarthNode->AddLayer(demLayer);
    pEarthNode->Initialize();

    
    earthcore::EarthRenderer* earthRender = new earthcore::EarthRenderer();
    earthRender->AddMaterial(material);
    pEarthNode->AddComponent(earthRender);

    mSceneManager->getRootNode()->AddSceneNode(pEarthNode);
    
}
