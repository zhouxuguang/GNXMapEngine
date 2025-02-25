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
#include "imagecodec/ColorConverter.h"

#include "WebMercator.h"
//#include "httplib.h"
#include "earthCore/Ellipsoid.h"
#include "earthCore/GeoGridTessellator.h"
#include "earthCore/EarthNode.h"
#include "earthCore/EarthCamera.h"
#include "earthCore/QuadTree.h"

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
    
    Texture2DPtr texture = getRenderDevice()->createTextureWithDescriptor(textureDescriptor);
    Rect2D rect(0, 0, image->GetWidth(), image->GetHeight());
    texture->replaceRegion(rect, image->GetPixels());
    return texture;
}

MapRenderer::MapRenderer(void *metalLayer) : mTileLoadPool(4)
{
#ifdef _WIN32
    mRenderdevice = createRenderDevice(RenderDeviceType::VULKAN, metalLayer);
#elif __APPLE__
    mRenderdevice = createRenderDevice(RenderDeviceType::METAL, metalLayer);
#endif // _WIN

    
    mSceneManager = SceneManager::GetInstance();
    
    BuildEarthNode();

	auto    root0 = new earthcore::QuadNode(
		 nullptr
		, Vector2d(-M_PI, -M_PI_2)
		, Vector2d(0, M_PI_2)
		, 0
		, earthcore::QuadNode::CHILD_LT
	);
	auto    root1 = new earthcore::QuadNode(
        nullptr
		, Vector2d(0, -M_PI_2)
		, Vector2d(M_PI, M_PI_2)
		, 0
		, earthcore::QuadNode::CHILD_LT
	);
    
    // 开启异步加载数据的线程池
    mTileLoadPool.Start();
}

void MapRenderer::SetWindowSize(uint32_t width, uint32_t height)
{
    mWidth = width;
    mHeight = height;
    
    mRenderdevice->resize(width, height);
    
    earthcore::Ellipsoid wgs84 = earthcore::Ellipsoid::WGS84;
    mCameraPtr = std::make_shared<earthcore::EarthCamera>(wgs84, "MainCamera");
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

void MapRenderer::DrawFrame()
{
    if (!mRenderdevice)
    {
        return;
    }
    
    uint64_t thisTime = GetTickNanoSeconds();
    float deltaTime = float(thisTime - mLastTime) * 0.000000001f;
    printf("deltaTime = %f\n", deltaTime);
    mLastTime = thisTime;
    
    mSceneManager->Update(deltaTime);
    
    CommandBufferPtr commandBuffer = mRenderdevice->createCommandBuffer();
    RenderEncoderPtr renderEncoder = commandBuffer->createDefaultRenderEncoder();
    
    mSceneManager->Render(renderEncoder);
    
    renderEncoder->EndEncode();
    commandBuffer->presentFrameBuffer();
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
    Texture2DPtr texture = TextureFromFile(R"(D:\source\graphics\engine\GNXMapEngine\GNXMapEngine\asset\NaturalEarth\NE2_50M_SR_W.jpg)");
    material->SetTexture("diffuseTexture", texture);
    meshRender->AddMaterial(material);
    
    earthcore::EarthNode *pNode = new earthcore::EarthNode(wgs84);
    pNode->AddComponent(meshRender);
    
//    Matrix4x4f modelMat = Matrix4x4f::CreateRotation(1, 0, 0, 90) * Matrix4x4f::CreateRotation(0, 0, 1, 90);
//    Quaternionf rotate;
//    rotate.FromRotateMatrix(modelMat.GetMatrix3());
    
    mSceneManager->getRootNode()->AddSceneNode(pNode);
    
}
