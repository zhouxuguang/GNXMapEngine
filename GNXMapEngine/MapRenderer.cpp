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
#include "RenderSystem/ArcballManipulate.h"
#include "MathUtil/Vector3.h"
#include "RenderSystem/SkyBoxNode.h"
#include "ImageCodec/ImageDecoder.h"
#include "RenderSystem/RenderEngine.h"
#include "RenderSystem/ImageTextureUtil.h"
#include "BaseLib/DateTime.h"

#include "WebMercator.h"
#include "httplib.h"
#include "earthCore/Ellipsoid.h"
#include "earthCore/GeoGridTessellator.h"

#include <filesystem>

namespace fs = std::filesystem;

using namespace RenderCore;
using namespace RenderSystem;

MapRenderer::MapRenderer(void *metalLayer) : mTileLoadPool(4)
{
    mRenderdevice = createRenderDevice(RenderDeviceType::METAL, metalLayer);
    mSceneManager = SceneManager::GetInstance();
    
    {
        earthcore::Ellipsoid wgs84 = earthcore::Ellipsoid::WGS84;
        earthcore::Geodetic3D geodetic3D(0, 0, 0);
        Vector3d position = wgs84.CartographicToCartesian(geodetic3D);
        earthcore::Geodetic3D geodetic3D1 = wgs84.CartesianToCartographic(position);
        
        MeshPtr mesh = earthcore::GeoGridTessellator::Compute(wgs84, 360, 180, earthcore::GeoGridTessellator::GeoGridVertexAttributes::All);
        
        printf("");
    }
    
    ShaderAssetString shaderAssetString = LoadCustomShaderAsset("/Users/zhouxuguang/work/mycode/GNXMapEngine/GNXMapEngine/TileDraw.hlsl");
    
    ShaderCodePtr vertexShader = shaderAssetString.vertexShader->shaderSource;
    ShaderCodePtr fragmentShader = shaderAssetString.fragmentShader->shaderSource;
    ShaderFunctionPtr vertShader = mRenderdevice->createShaderFunction(*vertexShader, ShaderStage_Vertex);
    ShaderFunctionPtr fragShader = mRenderdevice->createShaderFunction(*fragmentShader, ShaderStage_Fragment);
    GraphicsPipelineDescriptor graphicsPipelineDescriptor;
    graphicsPipelineDescriptor.vertexDescriptor = shaderAssetString.vertexDescriptor;
    
//    mPipeline = mRenderdevice->createGraphicsPipeline(graphicsPipelineDescriptor);
//    mPipeline->attachVertexShader(vertShader);
//    mPipeline->attachFragmentShader(fragShader);
//    
//    // 创建纹理和采样器
//    SamplerDescriptor sampleDes;
//    mTexSampler = mRenderdevice->createSamplerWithDescriptor(sampleDes);
    
    // 开启异步加载数据的线程池
    mTileLoadPool.Start();
}

void MapRenderer::SetWindowSize(uint32_t width, uint32_t height)
{
    mWidth = width;
    mHeight = height;
    
    mRenderdevice->resize(width * 2, height * 2);
    
    CameraPtr cameraPtr = mSceneManager->createCamera("MainCamera");
    cameraPtr->LookAt(mathutil::Vector3f(0, 0, 5), mathutil::Vector3f(0, 0, 0), mathutil::Vector3f(0, 1, 0));
    cameraPtr->SetLens(60, float(width) / height, 0.1f, 100.f);
    
    //初始化灯光信息
    Light * pointLight = mSceneManager->createLight("mainLight", Light::LightType::PointLight);
    pointLight->setColor(Vector3f(1.0, 1.0, 1.0));
    //pointLight->setPosition(Vector3f(5.0, 8.0, 0.0));
    pointLight->setPosition(Vector3f(-1.0, -1.0, -1.0));
    pointLight->setFalloffStart(5);
    pointLight->setFalloffEnd(300);
    pointLight->setStrength(Vector3f(8.0, 8.0, 8.0));
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
