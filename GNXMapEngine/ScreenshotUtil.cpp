//
//  ScreenshotUtil.cpp
//  GNXMapEngine
//

#include "ScreenshotUtil.h"

#include "Runtime/RenderCore/include/CommandBuffer.h"
#include "Runtime/RenderCore/include/CommandQueue.h"
#include "Runtime/RenderCore/include/RenderDevice.h"
#include "Runtime/RenderCore/include/RenderPass.h"
#include "Runtime/RenderCore/include/RenderEncoder.h"
#include "Runtime/RenderCore/include/BlitEncoder.h"
#include "Runtime/RenderCore/include/RCBuffer.h"
#include "Runtime/RenderCore/include/RCTexture.h"
#include "Runtime/RenderCore/include/TextureFormat.h"
#include "Runtime/RenderSystem/include/UI/ImGuiRenderer.h"
#include "Runtime/ImageCodec/include/ImageEncoder.h"
#include "Runtime/BaseLib/include/LogService.h"

#include <algorithm>
#include <cstring>
#include <vector>

using namespace RenderCore;

namespace
{
    // D3D12 的 texture->buffer 行 pitch 需要 256 字节对齐；其它后端沿用同一约定，
    // 这样可以一份代码跨后端（VirtualTextureFeedback 亦是如此）。
    const uint32_t kRowPitchAlignment = 256;

    uint32_t AlignUp(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }
}

bool CaptureFrameToPng(RenderSystem::SceneManager* sceneManager,
                       const RenderSystem::ImGuiRendererPtr& imGuiRenderer,
                       uint32_t width,
                       uint32_t height,
                       const std::string& filePath)
{
    if (!sceneManager)
    {
        LOG_ERROR("CaptureFrameToPng: sceneManager is null");
        return false;
    }

    if (width == 0 || height == 0)
    {
        LOG_ERROR("CaptureFrameToPng: invalid size %ux%u", width, height);
        return false;
    }

    RenderDevicePtr device = GetRenderDevice();
    if (!device)
    {
        LOG_ERROR("CaptureFrameToPng: render device is null");
        return false;
    }

    // ---- 1) 离屏附件 ----
    // 颜色用 RGBA8（跨后端都有映射；注意 kTexFormatBGRA32 在 Metal 侧的
    // ConvertTextureFormatToMetal 里没有 case，会命中 assert）。
    // 深度模板与默认（上屏）编码器一致地共用一张 Depth32Float_Stencil8 纹理。
    // 颜色格式与上屏不同只会让引擎按格式缓存的 PSO 多编译一份，不会画错。
    RCTexture2DPtr colorTexture = device->CreateTexture2D(
        kTexFormatRGBA8, TextureUsage::TextureUsageRenderTarget, width, height, 1);
    if (!colorTexture)
    {
        LOG_ERROR("CaptureFrameToPng: create color texture failed (%ux%u)", width, height);
        return false;
    }

    RCTexture2DPtr depthStencilTexture = device->CreateTexture2D(
        kTexFormatDepth32FloatStencil8, TextureUsage::TextureUsageRenderTarget, width, height, 1);
    if (!depthStencilTexture)
    {
        LOG_ERROR("CaptureFrameToPng: create depth-stencil texture failed (%ux%u)", width, height);
        return false;
    }

    // GPU -> CPU 回读缓冲
    const uint32_t rowPitch = AlignUp(width * 4u, kRowPitchAlignment);
    RCBufferPtr stagingBuffer = device->CreateBuffer(
        RCBufferDesc(rowPitch * height, RCBufferUsage::TransferDst, StorageModeShared));
    if (!stagingBuffer)
    {
        LOG_ERROR("CaptureFrameToPng: create staging buffer failed (%u bytes)", rowPitch * height);
        return false;
    }

    CommandQueuePtr queue = device->GetCommandQueue(QueueType::Graphics, 0);
    if (!queue)
    {
        LOG_ERROR("CaptureFrameToPng: graphics queue is null");
        return false;
    }

    CommandBufferPtr commandBuffer = queue->CreateCommandBuffer();
    if (!commandBuffer)
    {
        LOG_ERROR("CaptureFrameToPng: create command buffer failed");
        return false;
    }

    // ---- 2) 渲染场景到离屏纹理 ----
    {
        RenderPass renderPass;
        // 必须显式设置：Metal 后端用 renderRegion 设置视口，
        // 留空会得到 0x0 视口，整个 Pass 什么都光栅化不出来（全黑）。
        renderPass.renderRegion = Rect2D(0, 0, static_cast<int>(width), static_cast<int>(height));

        RenderPassColorAttachmentPtr colorAttachment = std::make_shared<RenderPassColorAttachment>();
        colorAttachment->texture = colorTexture;
        colorAttachment->loadOp = ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment->storeOp = ATTACHMENT_STORE_OP_STORE;
        colorAttachment->clearColor = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        renderPass.colorAttachments.push_back(colorAttachment);

        RenderPassDepthAttachmentPtr depthAttachment = std::make_shared<RenderPassDepthAttachment>();
        depthAttachment->texture = depthStencilTexture;
        depthAttachment->loadOp = ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment->storeOp = ATTACHMENT_STORE_OP_DONT_CARE;
        // Reverse-Z 下默认清除值必须是 0.0（远处），否则深度测试方向会反
        depthAttachment->clearDepth = DepthConfig::GetDefaultClearDepth();
        renderPass.depthAttachment = depthAttachment;

        RenderPassStencilAttachmentPtr stencilAttachment = std::make_shared<RenderPassStencilAttachment>();
        stencilAttachment->texture = depthStencilTexture;
        stencilAttachment->loadOp = ATTACHMENT_LOAD_OP_CLEAR;
        stencilAttachment->storeOp = ATTACHMENT_STORE_OP_DONT_CARE;
        stencilAttachment->clearStencil = 0;
        renderPass.stencilAttachment = stencilAttachment;

        RenderEncoderPtr renderEncoder = commandBuffer->CreateRenderEncoder(renderPass);
        if (!renderEncoder)
        {
            LOG_ERROR("CaptureFrameToPng: create render encoder failed");
            return false;
        }

        // 与上屏路径使用同一套渲染逻辑
        sceneManager->Render(renderEncoder);

        // Forward 渲染路径不会自动绘制 UI（只有 Deferred 的 Present Pass 会），
        // 因此这里显式叠加，保证出图里的数值面板与屏幕一致。
        if (imGuiRenderer)
        {
            imGuiRenderer->Render(renderEncoder);
        }

        renderEncoder->EndEncode();
    }

    // ---- 3) 回读到 CPU ----
    {
        BlitEncoderPtr blitEncoder = commandBuffer->CreateBlitEncoder();
        if (!blitEncoder)
        {
            LOG_ERROR("CaptureFrameToPng: create blit encoder failed");
            return false;
        }

        blitEncoder->CopyTextureToBuffer(colorTexture,
                                        0, 0,
                                        mathutil::Vector2i(0, 0),
                                        mathutil::Vector2i(static_cast<int>(width), static_cast<int>(height)),
                                        stagingBuffer,
                                        0,
                                        rowPitch,
                                        0);
        blitEncoder->EndEncode();
    }

    commandBuffer->Submit();
    commandBuffer->WaitUntilCompleted();

    void* mapped = stagingBuffer->Map();
    if (!mapped)
    {
        LOG_ERROR("CaptureFrameToPng: map staging buffer failed");
        return false;
    }

    // 回读数据是 RGBA8、行 pitch 已按 256 字节对齐，只需跳过每行末尾的填充字节。
    // 纹理原点在左上角，逐行取出来即为「从上到下」的像素顺序，无需再做翻转。
    const size_t tightRowBytes = static_cast<size_t>(width) * 4u;
    std::vector<uint8_t> rgbaPixels(tightRowBytes * height);
    const uint8_t* base = static_cast<const uint8_t*>(mapped);
    for (uint32_t y = 0; y < height; ++y)
    {
        memcpy(rgbaPixels.data() + static_cast<size_t>(y) * tightRowBytes,
               base + static_cast<size_t>(y) * rowPitch,
               tightRowBytes);
    }
    stagingBuffer->Unmap();

    // ---- 4) 编码 PNG ----
    // VImage 只引用外部数据、不拷贝，所以 rgbaPixels 必须活到编码结束。
    imagecodec::VImage image(imagecodec::FORMAT_RGBA8, width, height, rgbaPixels.data());
    const bool encoded = imagecodec::ImageEncoder::EncodeFile(filePath.c_str(), image,
                                                             imagecodec::ImageStoreFormat::kPNG_Format, 100);
    if (!encoded)
    {
        LOG_ERROR("CaptureFrameToPng: encode png failed: %s", filePath.c_str());
        return false;
    }

    LOG_INFO("CaptureFrameToPng: saved %s (%ux%u)", filePath.c_str(), width, height);
    return true;
}
