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
#include "MathUtil/Vector3.h"
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

static Texture2DPtr TextureFromFile(const char *filename)
{
    if (filename == nullptr)
    {
        return nullptr;
    }
    
    imagecodec::VImagePtr image = std::make_shared<imagecodec::VImage>();
    if (!imagecodec::ImageDecoder::DecodeFile(filename, image.get()))
    {
        return nullptr;
    }

    if (image->GetFormat() == imagecodec::FORMAT_SRGB8)
    {
        imagecodec::VImagePtr dstImage = std::make_shared<imagecodec::VImage>();
        dstImage->SetImageInfo(imagecodec::FORMAT_SRGB8_ALPHA8, image->GetWidth(), image->GetHeight());
        dstImage->AllocPixels();
        imagecodec::ColorConverter::convert_RGB24toRGBA32(image, dstImage);
        image = dstImage;
    }
    
    TextureDescriptor textureDescriptor = RenderSystem::ImageTextureUtil::getTextureDescriptor(*image);
    textureDescriptor.mipmaped = true;
    
    Texture2DPtr texture = GetRenderDevice()->CreateTextureWithDescriptor(textureDescriptor);
    Rect2D rect(0, 0, image->GetWidth(), image->GetHeight());
    texture->ReplaceRegion(rect, image->GetPixels());
    return texture;
}

MapRenderer::MapRenderer(void *metalLayer)
{
#if OS_WINDOWS
    mRenderdevice = CreateRenderDevice(RenderDeviceType::VULKAN, metalLayer);
#elif OS_MACOS
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

void testPost(const RenderEncoderPtr &renderEncoder, const RenderTexturePtr texture)
{
    postProcessing->SetRenderTexture(texture);
    postProcessing->Process(renderEncoder);
}

void MapRenderer::TestAtmo()
{
    if (!transmittance_texture)
    {
        RenderCore::TextureDescriptor imagedes;
        imagedes.width = Atmosphere::TRANSMITTANCE_TEXTURE_WIDTH;
        imagedes.height = Atmosphere::TRANSMITTANCE_TEXTURE_HEIGHT;
        imagedes.format = kTexFormatRGBA32Float;
        transmittance_texture = mRenderdevice->CreateRenderTexture(imagedes);
        
        initPostResource(mRenderdevice);
    }
    if (!mPipeline)
    {
        ShaderAssetString shaderAssetString = LoadShaderAsset("Atmosphere/ComputeTransmittance");
            
        ShaderCodePtr vertexShader = shaderAssetString.vertexShader->shaderSource;
        ShaderCodePtr fragmentShader = shaderAssetString.fragmentShader->shaderSource;

        GraphicsShaderPtr graphicsShader = mRenderdevice->CreateGraphicsShader(*vertexShader, *fragmentShader);

        GraphicsPipelineDescriptor graphicsPipelineDescriptor;
        graphicsPipelineDescriptor.vertexDescriptor = shaderAssetString.vertexDescriptor;
        
        mPipeline = mRenderdevice->CreateGraphicsPipeline(graphicsPipelineDescriptor);
        mPipeline->AttachGraphicsShader(graphicsShader);
    }
    if (!mUBO)
    {
        size_t size = sizeof(Atmosphere::AtmosphereParameters);
        mUBO = mRenderdevice->CreateUniformBufferWithSize((uint32_t)size);
        AtmosphereModel* model = CreateAtmoModel();
        mUBO->SetData(&model->GetAtmosphereParameters(), 0, (uint32_t)size);
    }
    
    CommandBufferPtr commandBuffer = mRenderdevice->CreateCommandBuffer();
        
    RenderPass renderPass;
    RenderPassColorAttachmentPtr colorAttachmentPtr = std::make_shared<RenderPassColorAttachment>();
    colorAttachmentPtr->clearColor = MakeClearColor(0.0, 0.0, 0.0, 1.0);
    colorAttachmentPtr->texture = transmittance_texture;
    renderPass.colorAttachments.push_back(colorAttachmentPtr);

    renderPass.renderRegion = Rect2D(0, 0, Atmosphere::TRANSMITTANCE_TEXTURE_WIDTH, Atmosphere::TRANSMITTANCE_TEXTURE_HEIGHT);
    RenderEncoderPtr renderEncoder1 = commandBuffer->CreateRenderEncoder(renderPass);
    
    renderEncoder1->SetGraphicsPipeline(mPipeline);
    renderEncoder1->SetFragmentUniformBuffer(mUBO, 0);
    renderEncoder1->DrawPrimitves(PrimitiveMode_TRIANGLES, 0, 3);
    
    renderEncoder1->EndEncode();
    
    RenderEncoderPtr renderEncoder = commandBuffer->CreateDefaultRenderEncoder();
    testPost(renderEncoder, transmittance_texture);
    renderEncoder->EndEncode();
    commandBuffer->PresentFrameBuffer();
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
    
    MeshPtr mesh = earthcore::GeoGridTessellator::Compute(wgs84, 360, 180, earthcore::GeoGridTessellator::GeoGridVertexAttributes::All);
    
    MeshRenderer* meshRender = new(std::nothrow) MeshRenderer();
    meshRender->SetSharedMesh(mesh);
    
    MaterialPtr material = Material::GetDefaultDiffuseMaterial();
    
    fs::path filePath = fs::absolute(fs::path(__FILE__)).parent_path();
    filePath = (filePath / "asset/NaturalEarth/NE2_50M_SR_W.jpg").lexically_normal();
    Texture2DPtr texture = TextureFromFile(filePath.string().c_str());
    
    material->SetTexture("diffuseTexture", texture);
    meshRender->AddMaterial(material);

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

    //pEarthNode->AddComponent(meshRender);
    mSceneManager->getRootNode()->AddSceneNode(pEarthNode);
    
}
