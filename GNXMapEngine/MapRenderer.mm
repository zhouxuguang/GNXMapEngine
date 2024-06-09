//
//  MapRenderer.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#include "MapRenderer.h"
#include <MetalKit/MetalKit.h>
#include <QtCore>

#include "RenderCore/RenderDevice.h"
#include "RenderSystem/SceneManager.h"
#include "RenderSystem/SceneNode.h"
#include "RenderSystem/ArcballManipulate.h"
#include "MathUtil/Vector3.h"
#include "RenderSystem/SkyBoxNode.h"
#include "ImageCodec/ImageDecoder.h"
#include "RenderSystem/RenderEngine.h"
#include "BaseLib/DateTime.h"

// Main class performing the rendering
@implementation AAPLRenderer
{
    RenderDevicePtr mRenderdevice;
}

- (nonnull instancetype)initWithMetalLayer:(nonnull CAMetalLayer *)metalLayer library:(nonnull id<MTLLibrary>)library
{
    self = [super init];
    if(self)
    {
        mRenderdevice = createRenderDevice(RenderDeviceType::METAL, (__bridge void*)metalLayer);
        //mRenderdevice->resize(600, 400);
    }

    return self;
}
/// Called whenever the view needs to render a frame
- (void)drawFrame
{
    CommandBufferPtr commandBuffer = mRenderdevice->createCommandBuffer();
    RenderEncoderPtr renderEncoder = commandBuffer->createDefaultRenderEncoder();
    
    renderEncoder->EndEncode();
    commandBuffer->presentFrameBuffer();
}

@end
