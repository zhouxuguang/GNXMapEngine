//
//  ScreenshotUtil.h
//  GNXMapEngine
//
//  把当前场景离屏渲染一帧并保存为 PNG。
//
//  为什么走离屏而不是抓取上屏画面：
//    * MTLCommandBuffer 的 CAMetalDrawable 纹理没有对外接口，抓不到；
//    * 离屏附件可以显式指定格式，只要与上屏一致（Metal 下 CAMetalLayer 默认
//      BGRA8Unorm），引擎就能复用已经缓存的 PSO，出图与屏幕画面完全一致，
//      也不会产生额外的管线编译开销。
//
//  依赖的引擎能力均为公开接口，写法与 VirtualTextureFeedback（纹理回读）以及
//  SSAOPass / GBufferRenderer（离屏 RenderPass + CreateRenderEncoder）一致。
//

#ifndef GNX_MAP_ENGINE_SCREENSHOT_UTIL_INCLUDE_JKSDHFG
#define GNX_MAP_ENGINE_SCREENSHOT_UTIL_INCLUDE_JKSDHFG

#include "Runtime/RenderSystem/include/SceneManager.h"

#include <cstdint>
#include <string>

// 离屏渲染一帧并保存为 PNG。
//
//   sceneManager : 场景管理器（Forward 路径下 Render 会遍历场景节点）
//   imGuiRenderer: 可选的 UI 层；非空时会叠加绘制当前 ImGui 帧
//                  （调用方需先构建好面板并调用过 ImGui::Render()）
//   width/height : 输出尺寸，应为帧缓冲像素尺寸（与窗口的 GetWidth/GetHeight 一致）
//   filePath     : 输出 PNG 路径
//
// 返回是否成功。失败原因会通过 LOG_ERROR 输出。
bool CaptureFrameToPng(RenderSystem::SceneManager* sceneManager,
                       const RenderSystem::ImGuiRendererPtr& imGuiRenderer,
                       uint32_t width,
                       uint32_t height,
                       const std::string& filePath);

#endif /* GNX_MAP_ENGINE_SCREENSHOT_UTIL_INCLUDE_JKSDHFG */
