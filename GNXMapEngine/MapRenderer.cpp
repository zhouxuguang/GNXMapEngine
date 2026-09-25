//
//  MapRenderer.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#include "MapRenderer.h"
#include "Runtime/RenderSystem/include/SceneManager.h"
#include "Runtime/RenderSystem/include/SceneNode.h"
#include "Runtime/RenderSystem/include/mesh/MeshRenderer.h"
#include "Runtime/RenderSystem/include/SkyBoxNode.h"
#include "Runtime/ImageCodec/include/ImageDecoder.h"
#include "Runtime/RenderSystem/include/RenderEngine.h"
#include "Runtime/RenderSystem/include/ImageTextureUtil.h"
#include "Runtime/BaseLib/include/DateTime.h"
#include "Runtime/BaseLib/include/LogService.h"
#include "Runtime/ImageCodec/include/ColorConverter.h"
#include "Runtime/RenderCore/include/CommandQueue.h"

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
#include <cstdlib>

namespace fs = std::filesystem;

using namespace RenderCore;
using namespace RenderSystem;

MapRenderer::MapRenderer()
{
    mRenderdevice = GetRenderDevice();
    mSceneManager = SceneManager::GetInstance();
    mSceneManager->SetRenderPath(RenderPath::Forward);
    
    BuildEarthNode();
}

void MapRenderer::SetWindowSize(uint32_t width, uint32_t height)
{
    mWidth = width;
    mHeight = height;
    
    if (!mSceneManager->HasCamera(mCameraPtr->GetName()))
    {
        mSceneManager->AddCamera(mCameraPtr);
    }
    mCameraPtr->SetLens(60, width, height, 10, 6378137.0 * 4);
    
//    cameraPtr->LookAt(Vector3f(2, 0, 0), Vector3f(0, 0, 0), Vector3f(0, 0, 1));
//    cameraPtr->SetLens(60, float(width) / height, 0.1f, 100);
    
    //初始化灯光信息
    Light * pointLight = mSceneManager->GetLight("mainLight");
    if (!pointLight)
    {
        pointLight = mSceneManager->CreateLight("mainLight", Light::LightType::PointLight);
    }
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
    float deltaTime = mLastTime == 0 ? 0.0f : float(thisTime - mLastTime) * 0.000000001f;
    mLastTime = thisTime;
    
    mSceneManager->Update(deltaTime);
    
    CommandQueuePtr commandQueue = mRenderdevice->GetCommandQueue(QueueType::Graphics);
    CommandBufferPtr commandBuffer = commandQueue->CreateCommandBuffer();
    if (!commandBuffer)
    {
        return;
    }
    RenderEncoderPtr renderEncoder = commandBuffer->CreateDefaultRenderEncoder();
    
    mSceneManager->Render(renderEncoder);

    // 前向渲染路径下引擎不会自动绘制 UI（只有延迟渲染的 Present Pass 才会），
    // 这里显式补一次：ImGui 帧由 AppFrameWork 每帧 NewFrame 驱动，面板内容与
    // ImGui::Render() 由应用层负责。
    if (RenderSystem::ImGuiRendererPtr imguiRenderer = mSceneManager->PeekImGuiRenderer())
    {
        imguiRenderer->Render(renderEncoder);
    }
    
    renderEncoder->EndEncode();
    commandBuffer->PresentFrameBuffer();
}

// ==================== 视角控制：方位角 / 俯仰角 ====================

void MapRenderer::SetAzimuthPitchDegrees(double azimuthDegrees, double pitchDegrees)
{
    if (!mCameraPtr)
    {
        return;
    }

    mCameraPtr->SetAzimuthPitchDegrees(azimuthDegrees, pitchDegrees);
}

void MapRenderer::SetEyeDistance(double distance)
{
    if (!mCameraPtr)
    {
        return;
    }

    mCameraPtr->SetEyeDistance(distance);
}

double MapRenderer::GetEyeDistance() const
{
    return mCameraPtr ? mCameraPtr->GetEyeDistance() : 0.0;
}

double MapRenderer::GetAzimuthAngleDegrees() const
{
    return mCameraPtr ? mCameraPtr->GetAzimuthAngleDegrees() : 0.0;
}

double MapRenderer::GetPitchAngleDegrees() const
{
    return mCameraPtr ? mCameraPtr->GetPitchAngleDegrees() : 0.0;
}

double MapRenderer::GetAzimuthAngleAtTargetDegrees() const
{
    return mCameraPtr ? mCameraPtr->GetAzimuthAngleAtTargetDegrees() : 0.0;
}

double MapRenderer::GetPitchAngleAtTargetDegrees() const
{
    return mCameraPtr ? mCameraPtr->GetPitchAngleAtTargetDegrees() : 0.0;
}

void MapRenderer::SetTargetGeodeticDegrees(double longitudeDegrees, double latitudeDegrees, double heightMeters)
{
    if (!mCameraPtr)
    {
        return;
    }

    mCameraPtr->SetEyeGeodeticTarget(earthcore::Geodetic3D::FromDegrees(longitudeDegrees, latitudeDegrees, heightMeters));
}

void MapRenderer::GetEyeGeodeticDegrees(double& longitudeDegrees, double& latitudeDegrees, double& heightMeters) const
{
    if (!mCameraPtr)
    {
        longitudeDegrees = latitudeDegrees = heightMeters = 0.0;
        return;
    }

    const earthcore::Geodetic3D& eye = mCameraPtr->GetEyeGeodetic();
    longitudeDegrees = earthcore::EarthCameraPose::ToDegrees(eye.Longitude());
    latitudeDegrees = earthcore::EarthCameraPose::ToDegrees(eye.Latitude());
    heightMeters = eye.Height();
}

void MapRenderer::GetTargetGeodeticDegrees(double& longitudeDegrees, double& latitudeDegrees, double& heightMeters) const
{
    if (!mCameraPtr)
    {
        longitudeDegrees = latitudeDegrees = heightMeters = 0.0;
        return;
    }

    const earthcore::Geodetic3D& target = mCameraPtr->GetEyeGeodeticTarget();
    longitudeDegrees = earthcore::EarthCameraPose::ToDegrees(target.Longitude());
    latitudeDegrees = earthcore::EarthCameraPose::ToDegrees(target.Latitude());
    heightMeters = target.Height();
}

Vector3d MapRenderer::GetViewDirectionInEyeEun() const
{
    return mCameraPtr ? mCameraPtr->GetViewDirectionInEyeEun() : Vector3d(0.0, 0.0, -1.0);
}

Vector3f MapRenderer::GetCameraPosition() const
{
    return mCameraPtr ? mCameraPtr->GetPosition() : Vector3f(0.0f, 0.0f, 0.0f);
}

void MapRenderer::LogCameraState(const char* tag) const
{
    if (!mCameraPtr)
    {
        return;
    }

    double eyeLon = 0.0, eyeLat = 0.0, eyeHeight = 0.0;
    double targetLon = 0.0, targetLat = 0.0, targetHeight = 0.0;
    GetEyeGeodeticDegrees(eyeLon, eyeLat, eyeHeight);
    GetTargetGeodeticDegrees(targetLon, targetLat, targetHeight);

    const Vector3d viewInEun = GetViewDirectionInEyeEun();

    LOG_INFO("[%s] 方位角(目标点基准)=%.6f 度 俯仰角(目标点基准)=%.6f 度 | "
             "方位角(视点基准)=%.6f 度 俯仰角(视点基准)=%.6f 度 | 距离=%.3f m\n"
             "        视点: 经度=%.6f 纬度=%.6f 高=%.3f\n"
             "        目标: 经度=%.6f 纬度=%.6f 高=%.3f\n"
             "        视线(东/北/天)=%.6f / %.6f / %.6f",
             tag,
             GetAzimuthAngleAtTargetDegrees(), GetPitchAngleAtTargetDegrees(),
             GetAzimuthAngleDegrees(), GetPitchAngleDegrees(),
             GetEyeDistance(),
             eyeLon, eyeLat, eyeHeight,
             targetLon, targetLat, targetHeight,
             viewInEun.x, viewInEun.y, viewInEun.z);
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
#if GNX_OS_MACOS
    fs::path dataPath = R"(/Users/zhouxuguang/work/data/gis/tile/image)";
    fs::path demPath = R"(/Users/zhouxuguang/work/data/gis/tile/terrain)";
#elif GNX_OS_WINDOWS
    fs::path dataPath = R"(D:/source/gis/data/tile/image)";
    //fs::path demPath = R"(D:/source/gis/data/tile/terrain)";
    fs::path demPath = R"(D:/source/gis/gdal/cesium-terrain-builder/build/Debug/terrain-tiles/test)";
#endif

    const char* imageTileOverride = std::getenv("GNX_MAP_IMAGE_TILES");
    const char* terrainTileOverride = std::getenv("GNX_MAP_TERRAIN_TILES");
    if (imageTileOverride && imageTileOverride[0] != '\0')
    {
        dataPath = imageTileOverride;
    }
    if (terrainTileOverride && terrainTileOverride[0] != '\0')
    {
        demPath = terrainTileOverride;
    }
    earthcore::TileDataSourcePtr imageSource = std::make_shared<earthcore::TileDataSource>(dataPath.string(), "jpg");
    earthcore::LayerBasePtr imageLayer = std::make_shared<earthcore::LayerBase>("Image", earthcore::LT_Image);
    imageLayer->SetDataSource(imageSource);

	earthcore::TileDataSourcePtr demSource = std::make_shared<earthcore::TileDataSource>(demPath.string(), "terrain");
	earthcore::LayerBasePtr demLayer = std::make_shared<earthcore::LayerBase>("DEM", earthcore::LT_Terrain);
    demLayer->SetDataSource(demSource);
    
    pEarthNode->AddLayer(imageLayer);
    pEarthNode->AddLayer(demLayer);
    pEarthNode->Initialize();

    
    earthcore::EarthRenderer* earthRender = pEarthNode->AddComponent<earthcore::EarthRenderer>();
    earthRender->AddMaterial(material);

    mSceneManager->GetRootNode()->AddSceneNode(pEarthNode);
    
}
