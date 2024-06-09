//
//  MapRenderer.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#include "MapRenderer.h"
#include <MetalKit/MetalKit.h>
#include <QtCore>

// Main class performing the rendering
@implementation AAPLRenderer
{
    // The device (aka GPU) we're using to render
    id <MTLDevice> _device;

    // Our render pipeline composed of our vertex and fragment shaders in the .metal shader file
    id<MTLRenderPipelineState> _pipelineState;

    // The command Queue from which we'll obtain command buffers
    id <MTLCommandQueue> _commandQueue;

    CAMetalLayer * _metalLayer;
}

- (nonnull instancetype)initWithMetalLayer:(nonnull CAMetalLayer *)metalLayer library:(nonnull id<MTLLibrary>)library
{
    self = [super init];
    if(self)
    {
        NSError *error = NULL;
        
        _metalLayer = metalLayer;
        
        _device = _metalLayer.device;

        // Indicate we would like to use the RGBAPisle format.
        _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;

        // Create the command queue
        _commandQueue = [_device newCommandQueue];
    }

    return self;
}
/// Called whenever the view needs to render a frame
- (void)drawFrame
{
    // Create a new command buffer for each renderpass to the current drawable
    id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    commandBuffer.label = @"MyCommand";

    @autoreleasepool { // Make sure the CAMetalDrawable is released immediately, as per docs

    // Obtain a renderPassDescriptor generated from the view's drawable textures
    id<CAMetalDrawable> drawable = [_metalLayer nextDrawable];
    if (drawable == nil)
        return;
    
    MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(1.0, 0.0, 0.0, 1.0);
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    
    // Create a render command encoder so we can render into something
    id <MTLRenderCommandEncoder> renderEncoder =
    [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    renderEncoder.label = @"MyRenderEncoder";

    // Set the region of the drawable to which we'll draw.
    [renderEncoder setViewport:(MTLViewport){0.0, 0.0, _metalLayer.drawableSize.width, _metalLayer.drawableSize.height, -1.0, 1.0 }];

    [renderEncoder endEncoding];

    // Schedule a present once the framebuffer is complete using the current drawable
    [commandBuffer presentDrawable:drawable];

    // Finalize rendering here & push the command buffer to the GPU
    [commandBuffer commit];
    
    } // autoreleasepool
}

@end
