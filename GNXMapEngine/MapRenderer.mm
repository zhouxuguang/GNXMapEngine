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
#include "BaseLib/DateTime.h"

MapRenderer::MapRenderer(CAMetalLayer *metalLayer)
{
    mRenderdevice = createRenderDevice(RenderDeviceType::METAL, (__bridge void*)metalLayer);
    //mRenderdevice->resize(600, 400);
    mProjection = Matrix4x4f::CreateOrthographic(-20037508, 20037508, -20037508, 20037508, -100.0f, 100.0f);
}


void MapRenderer::DrawFrame()
{
    CommandBufferPtr commandBuffer = mRenderdevice->createCommandBuffer();
    RenderEncoderPtr renderEncoder = commandBuffer->createDefaultRenderEncoder();
    
    renderEncoder->EndEncode();
    commandBuffer->presentFrameBuffer();
}

