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

#include <filesystem>

namespace fs = std::filesystem;

using namespace RenderCore;
using namespace RenderSystem;

void TileLoadTask::Run()
{
    TileDataPtr cachedPtr = mRender->GetTileDataFromCache(tileKey);
    if (cachedPtr)
    {
        mRender->PushTileData(cachedPtr);
        return;
    }
    
    int x = tileKey.x;
    int y = tileKey.y;
    int level = tileKey.level;
    
    // 当前级别的瓦片个数
    int  nTileCount = 1 << level;
    
    // 瓦片重复的逻辑
    int  tmpX = x;
    int  tmpY = y;
    {
        if (x < 0)
        {
            tmpX  =   (x % nTileCount + nTileCount) % nTileCount;
        }
        else
        {
            tmpX  %=  nTileCount;
        }
        if (y < 0)
        {
            tmpY  =   (y % nTileCount + nTileCount) % nTileCount;
        }
        else
        {
            tmpY  %=  nTileCount;
        }
    }
    
    TileDataPtr tileData = std::make_shared<TileData>();
    tileData->key = Vector2i(x, y);
    
    tileData->start = WebMercator::tileToWorld(Vector2i(x, y), level);
    tileData->end = WebMercator::tileToWorld(Vector2i(x + 1, y + 1), level);
    
    char filePath[1024] = {0};
    snprintf(filePath, 1024, "/Users/zhouxuguang/work/mycode/GNXMapEngine/GNXMapEngine/data/L%02d/%06d-%06d.jpg", level, tmpY, tmpX);
    
    // 文件缓存存在
    if (fs::exists(filePath))
    {
        VImage image;
        bool bRet = ImageDecoder::DecodeFile(filePath, &image);
        if (bRet)
        {
            TextureDescriptor texDes = ImageTextureUtil::getTextureDescriptor(image);
            tileData->texture = getRenderDevice()->createTextureWithDescriptor(texDes);
            tileData->texture->setTextureData(image.GetPixels());
        }
        else
        {
            tileData->texture = nullptr;
        }
    }
    else
    {
        // 下载图像并创建纹理
        char imagePath[1024] = {0};
        snprintf(imagePath, 1024, "https://gac-geo.googlecnapps.club/maps/vt?lyrs=s&x=%d&y=%d&z=%d", tmpX, tmpY, level);
        
        std::vector<uint8_t> body;
        body.reserve(4096);
        httplib::Client client("gac-geo.googlecnapps.club");
        auto res = client.Get(imagePath,
          [&](const char *data, size_t dataLength)
        {
            body.insert(body.end(), data, data + dataLength);
            return true;
        });
        
        VImage image;
        bool bRet = ImageDecoder::DecodeMemory(body.data(), body.size(), &image);
        if (bRet)
        {
            TextureDescriptor texDes = ImageTextureUtil::getTextureDescriptor(image);
            tileData->texture = getRenderDevice()->createTextureWithDescriptor(texDes);
            tileData->texture->setTextureData(image.GetPixels());
            
            char fileDirPath[1024] = {0};
            snprintf(fileDirPath, 1024, "/Users/zhouxuguang/work/mycode/GNXMapEngine/GNXMapEngine/data/L%02d", level);
            fs::create_directories(fileDirPath);
            
            FILE* fp = fopen(filePath, "wb");
            fwrite(body.data(), 1, body.size(), fp);
            fclose(fp);
        }
        else
        {
            tileData->texture = nullptr;
        }
    }
    mRender->PutTileDataToCache(tileKey, tileData);
    
    
    //创建顶点缓冲区
    struct Vertex
    {
        Vector2f pos[4];
        Vector2f tex[4];
    };
    
    Vertex  vertex;
    vertex.pos[0] = Vector2f(tileData->start.x, tileData->start.y);
    vertex.pos[1] = Vector2f(tileData->end.x, tileData->start.y);
    vertex.pos[2] = Vector2f(tileData->start.x, tileData->end.y);
    vertex.pos[3] = Vector2f(tileData->end.x, tileData->end.y);
    
    vertex.tex[0] = Vector2f(0,0);
    vertex.tex[1] = Vector2f(1,0);
    vertex.tex[2] = Vector2f(0,1);
    vertex.tex[3] = Vector2f(1,1);
    
    
    tileData->vertexBuffer = getRenderDevice()->createVertexBufferWithBytes(&vertex, sizeof(vertex), StorageModePrivate);
    
    mRender->PushTileData(tileData);
}

MapRenderer::MapRenderer(void *metalLayer) : mTileCache(200), mTileLoadPool(4)
{
    mRenderdevice = createRenderDevice(RenderDeviceType::METAL, metalLayer);
    mProjection = Matrix4x4f::CreateOrthographic(-20037508, 20037508, -20037508, 20037508, -100.0f, 100.0f);
    
    ShaderAssetString shaderAssetString = LoadCustomShaderAsset("/Users/zhouxuguang/work/mycode/GNXMapEngine/GNXMapEngine/TileDraw.hlsl");
    
    ShaderCodePtr vertexShader = shaderAssetString.vertexShader->shaderSource;
    ShaderCodePtr fragmentShader = shaderAssetString.fragmentShader->shaderSource;
    ShaderFunctionPtr vertShader = mRenderdevice->createShaderFunction(*vertexShader, ShaderStage_Vertex);
    ShaderFunctionPtr fragShader = mRenderdevice->createShaderFunction(*fragmentShader, ShaderStage_Fragment);
    GraphicsPipelineDescriptor graphicsPipelineDescriptor;
    graphicsPipelineDescriptor.vertexDescriptor = shaderAssetString.vertexDescriptor;
    
    mPipeline = mRenderdevice->createGraphicsPipeline(graphicsPipelineDescriptor);
    mPipeline->attachVertexShader(vertShader);
    mPipeline->attachFragmentShader(fragShader);

    mUBO = mRenderdevice->createUniformBufferWithSize(sizeof(mProjection));
    mUBO->setData(&mProjection, 0, sizeof(mProjection));
    
    // 创建纹理和采样器
    SamplerDescriptor sampleDes;
    mTexSampler = mRenderdevice->createSamplerWithDescriptor(sampleDes);
    
    // 开启异步加载数据的线程池
    mTileLoadPool.Start();
}


void MapRenderer::DrawFrame()
{
    if (!mRenderdevice)
    {
        return;
    }
    
    mUBO->setData(&mProjection, 0, sizeof(mProjection));
    
    CommandBufferPtr commandBuffer = mRenderdevice->createCommandBuffer();
    RenderEncoderPtr renderEncoder = commandBuffer->createDefaultRenderEncoder();
    
    renderEncoder->setGraphicsPipeline(mPipeline);
    
    mTileDataLock.lock();
    TileDataArray tileSet = mTileDatas;
    mTileDataLock.unlock();
    
    printf("current draw tile count = %d\n", (int)tileSet.size());
    
    for (const auto &iter : tileSet)
    {
        if (!iter)
        {
            continue;
        }
        DrawTile(renderEncoder, *iter);
    }
    
    renderEncoder->EndEncode();
    commandBuffer->presentFrameBuffer();
}

void MapRenderer::DrawTile(RenderEncoderPtr renderEncoder, const TileData& tileData)
{
    if (!tileData.texture)
    {
        return;
    }
    renderEncoder->setVertexBuffer(tileData.vertexBuffer, 0, 0);
    renderEncoder->setVertexBuffer(tileData.vertexBuffer, 32, 1);
    renderEncoder->setVertexUniformBuffer(mUBO, 0);
    renderEncoder->setFragmentTextureAndSampler(tileData.texture, mTexSampler, 0);
    renderEncoder->drawPrimitves(PrimitiveMode_TRIANGLE_STRIP, 0, 4);
}

int MapRenderer::GetLevel()
{
    int  wRes = (mRight - mLeft) / mWidth;
    for (int i = 0; i < 22; ++i)
    {
        int res = (int)WebMercator::resolution(i);
        if (wRes > res)
        {
            return  i;
        }
    }
    
    return 22;
}

void MapRenderer::RequestTiles()
{
    // 计算左上和右下角的瓦片编号
    int level = GetLevel();
    Vector2i key1 = WebMercator::getKeyByMeter(level, mLeft, mTop);
    Vector2i key2 = WebMercator::getKeyByMeter(level, mRight, mBottom);

    int startX  =  std::min(key1.x, key2.x);
    int endX    =  std::max(key1.x, key2.x);

    int startY  =  std::min(key1.y, key2.y);
    int endY    =  std::max(key1.y, key2.y);
    
    TileDataArray tempTileDatas;
    
    mTileDataLock.lock();
    tempTileDatas = mTileDatas;
    mTileDatas.clear();
    mTileDataLock.unlock();
    
    TileDataArray inScreenTiles;
    
    // 取消还没有执行的任务
    mTileLoadPool.CancelAllTasks();

    // 循环请求各个瓦片
    for (int x = startX; x <= endX; ++ x)
    {
        for (int y = startY; y <= endY; ++ y)
        {
            TileKey tileKey;
            tileKey.level = level;
            tileKey.x = x;
            tileKey.y = y;
            
            bool isScreen = false;
            
            // 如果当前瓦片已经在上次显示的屏幕中，那么就跳过了，不需要重复加载
            for (int i = 0; i < tempTileDatas.size(); i ++)
            {
                TileKey key;
                key.level = level;
                key.x = tempTileDatas[i]->key.x;
                key.y = tempTileDatas[i]->key.y;
                if (key == tileKey)
                {
                    inScreenTiles.push_back(tempTileDatas[i]);
                    isScreen = true;
                    break;
                }
            }
            
            if (isScreen)
            {
                continue;
            }
            
            std::shared_ptr<TileLoadTask> tileLoadTask = std::make_shared<TileLoadTask>();
            tileLoadTask->mRender = this;
            tileLoadTask->tileKey = tileKey;
            
            mTileLoadPool.Execute(tileLoadTask);
        }
    }
    
    mTileDataLock.lock();
    mTileDatas = inScreenTiles;
    mTileDataLock.unlock();
}

