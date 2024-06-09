//
//  MapRenderer.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#include "MapRenderer.h"
#include <MetalKit/MetalKit.h>
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

using namespace RenderCore;
using namespace RenderSystem;

MapRenderer::MapRenderer(CAMetalLayer *metalLayer)
{
    mRenderdevice = createRenderDevice(RenderDeviceType::METAL, (__bridge void*)metalLayer);
    //mRenderdevice->resize(600, 400);
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
    
    // 初始化顶点缓冲
    struct Vertex
    {
        Vector2f pos[4];
        Vector2f tex[4];
    };
    
    
    float   x   =   -20037508;
    float   y   =   -20037508;
    float   w   =   20037508 * 2;
    float   h   =   20037508 * 2;
    
    Vertex  vertex;
    vertex.pos[0] = Vector2f(x,y);
    vertex.pos[1] = Vector2f(x + w,y);
    vertex.pos[2] = Vector2f(x,y + h);
    vertex.pos[3] = Vector2f(x + w, y + h);
    
    vertex.tex[0] = Vector2f(0,1);
    vertex.tex[1] = Vector2f(1,1);
    vertex.tex[2] = Vector2f(0,0);
    vertex.tex[3] = Vector2f(1,0);
    
    
    mVertexBuffer = mRenderdevice->createVertexBufferWithBytes(&vertex, sizeof(vertex), StorageModePrivate);
    mUBO = mRenderdevice->createUniformBufferWithSize(sizeof(mProjection));
    mUBO->setData(&mProjection, 0, sizeof(mProjection));
    
    // 创建纹理和采样器
    SamplerDescriptor sampleDes;
    mTexSampler = mRenderdevice->createSamplerWithDescriptor(sampleDes);
    
    VImage image;
    ImageDecoder::DecodeFile("/Users/zhouxuguang/work/mycode/GNXMapEngine/GNXMapEngine/world.jpeg", &image);
    
    TextureDescriptor texDes = ImageTextureUtil::getTextureDescriptor(image);
    mTexture = mRenderdevice->createTextureWithDescriptor(texDes);
    mTexture->setTextureData(image.GetPixels());
}


void MapRenderer::DrawFrame()
{
    if (!mRenderdevice)
    {
        return;
    }
    
    CommandBufferPtr commandBuffer = mRenderdevice->createCommandBuffer();
    RenderEncoderPtr renderEncoder = commandBuffer->createDefaultRenderEncoder();
    
    renderEncoder->setGraphicsPipeline(mPipeline);
#if 0
    renderEncoder->setVertexBuffer(mVertexBuffer, 0, 0);
    renderEncoder->setVertexBuffer(mVertexBuffer, 32, 1);
    renderEncoder->setVertexUniformBuffer(mUBO, 0);
    renderEncoder->setFragmentTextureAndSampler(mTexture, mTexSampler, 0);
    renderEncoder->drawPrimitves(PrimitiveMode_TRIANGLE_STRIP, 0, 4);
#endif
    
    for (const auto &iter : mTileDatas)
    {
        DrawTile(renderEncoder, iter);
    }
    
    renderEncoder->EndEncode();
    commandBuffer->presentFrameBuffer();
}

void MapRenderer::DrawTile(RenderEncoderPtr renderEncoder, const TileData& tileData)
{
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

    // 循环请求各个瓦片
    for (int x = startX; x <= endX; ++ x)
    {
        for (int y = startY; y <= endY; ++ y)
        {
            TileData tileData;
            tileData.key = Vector2i(x,y);
            tileData.start = WebMercator::tileToWorld(Vector2i(x,y), level);
            tileData.end = WebMercator::tileToWorld(Vector2i(x + 1, y + 1), level);
            
            
            // 下载图像并创建纹理
            char imagePath[1024] = {0};
            snprintf(imagePath, 1024, "https://gac-geo.googlecnapps.club/maps/vt?lyrs=s&x=%d&y=%d&z=%d", x, y, level);
            
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
                tileData.texture = mRenderdevice->createTextureWithDescriptor(texDes);
                tileData.texture->setTextureData(image.GetPixels());
            }
            else
            {
                tileData.texture = nullptr;
            }
            
            //创建顶点缓冲区
            struct Vertex
            {
                Vector2f pos[4];
                Vector2f tex[4];
            };
            
            Vertex  vertex;
            vertex.pos[0] = Vector2f(tileData.start.x, tileData.start.y);
            vertex.pos[1] = Vector2f(tileData.end.x, tileData.start.y);
            vertex.pos[2] = Vector2f(tileData.start.x, tileData.end.y);
            vertex.pos[3] = Vector2f(tileData.end.x, tileData.end.y);
            
            vertex.tex[0] = Vector2f(0,0);
            vertex.tex[1] = Vector2f(1,0);
            vertex.tex[2] = Vector2f(0,1);
            vertex.tex[3] = Vector2f(1,1);
            
            
            tileData.vertexBuffer = mRenderdevice->createVertexBufferWithBytes(&vertex, sizeof(vertex), StorageModePrivate);
            
            mTileDatas.push_back(tileData);
        }
    }
}

