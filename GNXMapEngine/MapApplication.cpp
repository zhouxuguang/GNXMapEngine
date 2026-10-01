#include "MapApplication.h"
#include "MapRenderer.h"
#include "earthCore/QuadTree.h"
#include "Runtime/GNXEngine/include/Input.h"
#include "Runtime/GNXEngine/include/RenderWindow.h"
#include "Runtime/BaseLib/include/LogService.h"

#include <imgui.h>

#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <string>

namespace
{
    // 读取环境变量：未设置或为空返回 false（保持默认值）
    bool GetEnvString(const char* name, std::string& out)
    {
        const char* value = std::getenv(name);
        if (!value || value[0] == '\0')
        {
            return false;
        }
        out = value;
        return true;
    }

    // 读取浮点环境变量：非法值回退到默认并打印错误，避免启动崩溃
    bool GetEnvDouble(const char* name, double& out)
    {
        const char* value = std::getenv(name);
        if (!value || value[0] == '\0')
        {
            return false;
        }

        char* end = nullptr;
        const double parsed = std::strtod(value, &end);
        if (end == value || (end != nullptr && *end != '\0') || !std::isfinite(parsed))
        {
            LOG_ERROR("环境变量 %s 的值 \"%s\" 不是合法数字，已忽略", name, value);
            return false;
        }

        out = parsed;
        return true;
    }

    // 读取整型环境变量：非法值回退到默认并打印错误
    bool GetEnvInt(const char* name, int& out)
    {
        const char* value = std::getenv(name);
        if (!value || value[0] == '\0')
        {
            return false;
        }

        char* end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        if (end == value || (end != nullptr && *end != '\0'))
        {
            LOG_ERROR("环境变量 %s 的值 \"%s\" 不是合法整数，已忽略", name, value);
            return false;
        }

        out = static_cast<int>(parsed);
        return true;
    }

    bool GetEnvBool(const char* name, bool defaultValue)
    {
        const char* value = std::getenv(name);
        if (!value || value[0] == '\0')
        {
            return defaultValue;
        }

        const std::string text(value);
        if (text == "0" || text == "false" || text == "FALSE" || text == "off" || text == "OFF")
        {
            return false;
        }
        return true;
    }
}

MapApplication::MapApplication(const GNXEngine::WindowProps& props)
    : AppFrameWork(props)
{
}

MapApplication::~MapApplication() = default;

void MapApplication::Initlize()
{
    AppFrameWork::Initlize();
    mRenderer = std::make_unique<MapRenderer>();

    // 先读启动开关，再决定是否创建 UI 层（关闭时不会创建任何 ImGui 资源）
    mPanelVisible = GetEnvBool("GNX_MAP_PANEL", true);
    SetImGuiEnabled(mPanelVisible);

    if (RenderSystem::ImGuiRendererPtr imgui = GetImGui())
    {
        if (const GNXEngine::RenderWindowPtr window = GNXEngine::GetRenderWindow())
        {
            imgui->SetDPIScale(window->GetDPIScale());
        }
    }

    ApplyStartupOptions();
}

void MapApplication::ApplyStartupOptions()
{
    if (!mRenderer)
    {
        return;
    }

    mRenderer->SetAtmosphereEnabled(GetEnvBool("GNX_MAP_ATMOSPHERE", true));

    // 顺序很重要：先定目标点，再定距离，最后定角度
    double targetLongitude = 0.0;
    double targetLatitude = 0.0;
    double targetHeight = 0.0;
    const bool hasTargetLongitude = GetEnvDouble("GNX_MAP_TARGET_LON", targetLongitude);
    const bool hasTargetLatitude = GetEnvDouble("GNX_MAP_TARGET_LAT", targetLatitude);
    GetEnvDouble("GNX_MAP_TARGET_HEIGHT", targetHeight);

    if (hasTargetLongitude && hasTargetLatitude)
    {
        mRenderer->SetTargetGeodeticDegrees(targetLongitude, targetLatitude, targetHeight);
    }
    else if (hasTargetLongitude || hasTargetLatitude)
    {
        LOG_ERROR("GNX_MAP_TARGET_LON 与 GNX_MAP_TARGET_LAT 必须成对设置，已忽略目标点设置");
    }

    double eyeDistance = 0.0;
    if (GetEnvDouble("GNX_MAP_EYE_DISTANCE", eyeDistance))
    {
        mRenderer->SetEyeDistance(eyeDistance);
    }

    double azimuthDegrees = 0.0;
    double pitchDegrees = 0.0;
    const bool hasAzimuth = GetEnvDouble("GNX_MAP_AZIMUTH", azimuthDegrees);
    const bool hasPitch = GetEnvDouble("GNX_MAP_PITCH", pitchDegrees);
    if (hasAzimuth || hasPitch)
    {
        // 只给出其中一个时，另一个沿用当前值
        if (!hasAzimuth)
        {
            azimuthDegrees = mRenderer->GetAzimuthAngleAtTargetDegrees();
        }
        if (!hasPitch)
        {
            pitchDegrees = mRenderer->GetPitchAngleAtTargetDegrees();
        }
        mRenderer->SetAzimuthPitchDegrees(azimuthDegrees, pitchDegrees);
    }
    mDefaultAzimuthDegrees = mRenderer->GetAzimuthAngleAtTargetDegrees();
    mDefaultPitchDegrees = mRenderer->GetPitchAngleAtTargetDegrees();

    // 拖拽灵敏度（1.0 为默认，负值反向，0 表示不响应），免重编译微调手感
    GetEnvDouble("GNX_MAP_AZIMUTH_SENSITIVITY", mAzimuthDragSensitivity);
    GetEnvDouble("GNX_MAP_PITCH_SENSITIVITY", mPitchDragSensitivity);
    GetEnvDouble("GNX_MAP_ZOOM_SENSITIVITY", mZoomDragSensitivity);
    mRenderer->SetDragSensitivity(mAzimuthDragSensitivity, mPitchDragSensitivity, mZoomDragSensitivity);

    // 自动化截图
    GetEnvString("GNX_MAP_SCREENSHOT", mScreenshotPath);
    GetEnvInt("GNX_MAP_SCREENSHOT_FRAMES", mScreenshotWaitFrames);
    if (mScreenshotWaitFrames < 1)
    {
        mScreenshotWaitFrames = 1;
    }

    LOG_INFO("启动配置: 面板=%s 方位角=%.6f 度 俯仰角=%.6f 度 距离=%.3f m 截图=%s 等待帧数=%d "
             "拖拽灵敏度(方位角/俯仰角/缩放)=%.3f/%.3f/%.3f",
             mPanelVisible ? "开" : "关",
             mRenderer->GetAzimuthAngleAtTargetDegrees(),
             mRenderer->GetPitchAngleAtTargetDegrees(),
             mRenderer->GetEyeDistance(),
             mScreenshotPath.empty() ? "(无)" : mScreenshotPath.c_str(),
             mScreenshotWaitFrames,
             mAzimuthDragSensitivity, mPitchDragSensitivity, mZoomDragSensitivity);

    mRenderer->LogCameraState("启动");

    ApplyCameraAnimationOptions();
}

void MapApplication::ApplyCameraAnimationOptions()
{
    if (!mRenderer || !GetEnvString("GNX_MAP_ANIM_DIR", mAnim.outputDir))
    {
        return;
    }

    mAnim.enabled = true;

    GetEnvInt("GNX_MAP_ANIM_FRAMES", mAnim.totalFrames);
    GetEnvInt("GNX_MAP_ANIM_CAPTURE_EVERY", mAnim.captureEvery);
    if (mAnim.totalFrames < 1)
    {
        mAnim.totalFrames = 1;
    }
    if (mAnim.captureEvery < 1)
    {
        mAnim.captureEvery = 1;
    }

    // 未设置的端点沿用当前相机状态。
    const double currentDistance = mRenderer->GetEyeDistance();
    const double currentAzimuth = mRenderer->GetAzimuthAngleAtTargetDegrees();
    const double currentPitch = mRenderer->GetPitchAngleAtTargetDegrees();

    mAnim.fromDistance = currentDistance;
    mAnim.toDistance = currentDistance;
    mAnim.fromAzimuth = currentAzimuth;
    mAnim.toAzimuth = currentAzimuth;
    mAnim.fromPitch = currentPitch;
    mAnim.toPitch = currentPitch;

    GetEnvDouble("GNX_MAP_ANIM_FROM_DISTANCE", mAnim.fromDistance);
    GetEnvDouble("GNX_MAP_ANIM_TO_DISTANCE", mAnim.toDistance);
    GetEnvDouble("GNX_MAP_ANIM_FROM_AZIMUTH", mAnim.fromAzimuth);
    GetEnvDouble("GNX_MAP_ANIM_TO_AZIMUTH", mAnim.toAzimuth);
    GetEnvDouble("GNX_MAP_ANIM_FROM_PITCH", mAnim.fromPitch);
    GetEnvDouble("GNX_MAP_ANIM_TO_PITCH", mAnim.toPitch);
    GetEnvDouble("GNX_MAP_ANIM_PAN_X", mAnim.panXPerFrame);
    GetEnvDouble("GNX_MAP_ANIM_PAN_Y", mAnim.panYPerFrame);

    if (mAnim.toDistance <= 0.0)
    {
        mAnim.toDistance = mAnim.fromDistance;
    }
    if (mAnim.fromDistance <= 0.0 || mAnim.toDistance <= 0.0)
    {
        LOG_ERROR("相机动画距离非法，已关闭动画自动化");
        mAnim.enabled = false;
        return;
    }

    // 第一帧使用动画起点。
    mRenderer->SetAzimuthPitchDegrees(mAnim.fromAzimuth, mAnim.fromPitch);
    mRenderer->SetEyeDistance(mAnim.fromDistance);

    LOG_INFO("相机动画开启: %d 帧, 每 %d 帧抓一张, 输出目录=%s\n"
             "        距离 %.1f -> %.1f m\n"
             "        方位角 %.3f -> %.3f 度\n"
             "        俯仰角 %.3f -> %.3f 度, 每帧平移 (%.2f, %.2f) px",
             mAnim.totalFrames, mAnim.captureEvery, mAnim.outputDir.c_str(),
             mAnim.fromDistance, mAnim.toDistance,
             mAnim.fromAzimuth, mAnim.toAzimuth,
             mAnim.fromPitch, mAnim.toPitch, mAnim.panXPerFrame, mAnim.panYPerFrame);
}

void MapApplication::UpdateCameraAnimation()
{
    if (!mAnim.enabled || !mRenderer)
    {
        return;
    }

    // 按帧号推进，保证抓图可复现。
    const double t = mAnim.totalFrames == 1 ? 1.0
        : std::min(1.0, static_cast<double>(mFrameIndex - 1) / static_cast<double>(mAnim.totalFrames - 1));

    // 距离几何插值，角度线性插值。
    const double distance = mAnim.fromDistance * std::pow(mAnim.toDistance / mAnim.fromDistance, t);
    const double azimuth = mAnim.fromAzimuth + (mAnim.toAzimuth - mAnim.fromAzimuth) * t;
    const double pitch = mAnim.fromPitch + (mAnim.toPitch - mAnim.fromPitch) * t;

    const bool panning = mAnim.panXPerFrame != 0.0 || mAnim.panYPerFrame != 0.0;
    if (!panning)
    {
        mRenderer->SetAzimuthPitchDegrees(azimuth, pitch);
    }
    mRenderer->SetEyeDistance(distance);
    if (panning && mFrameIndex > 1)
    {
        mRenderer->Pan(static_cast<float>(mAnim.panXPerFrame),
                       static_cast<float>(mAnim.panYPerFrame));
    }
}

void MapApplication::Resize(uint32_t width, uint32_t height)
{
    AppFrameWork::Resize(width, height);
    if (mRenderer)
    {
        mRenderer->SetWindowSize(width, height);
    }
}

void MapApplication::RenderFrame()
{
    if (!mRenderer)
    {
        return;
    }

    // 帧号从 1 开始，在绘制前递增。
    ++mFrameIndex;

    // 框架每帧只调用 ImGui::NewFrame()，面板内容与 ImGui::Render() 由应用负责。
    // 即使不显示面板也必须结束帧，否则下一帧 NewFrame() 会断言失败。
    if (IsImGuiEnabled())
    {
        if (RenderSystem::ImGuiRendererPtr imgui = GetImGui())
        {
            if (imgui->IsInitialized())
            {
                if (mPanelVisible)
                {
                    BuildImGuiPanel();
                }
                ImGui::Render();
            }
        }
    }

    // 动画先于拖拽更新。
    UpdateCameraAnimation();

    // 拖拽增量逐帧轮询，必须在绘制前更新相机姿态
    UpdateDragInteraction();

    mRenderer->DrawFrame();

    UpdateAutomation();
}

void MapApplication::OnEvent(GNXEngine::Event& event)
{
    // 先交给框架：处理窗口事件，并让 ImGui 优先消费输入
    // （UI 命中时会置 event.handled = true）
    AppFrameWork::OnEvent(event);

    // UI 已经消费掉的事件不再传给地图交互，避免「拖滑条的同时平移地球」
    if (event.handled)
    {
        return;
    }

    GNXEngine::EventDispatcher dispatcher(event);
    dispatcher.Dispatch<GNXEngine::MouseButtonPressedEvent>(
        [this](GNXEngine::MouseButtonPressedEvent& e) { return OnMouseButtonPressed(e); });
    dispatcher.Dispatch<GNXEngine::MouseButtonReleasedEvent>(
        [this](GNXEngine::MouseButtonReleasedEvent& e) { return OnMouseButtonReleased(e); });
    // 拖拽增量不在这里处理，改由 UpdateDragInteraction 每帧轮询（见头文件说明）
    dispatcher.Dispatch<GNXEngine::MouseScrolledEvent>(
        [this](GNXEngine::MouseScrolledEvent& e) { return OnMouseScrolled(e); });
}

bool MapApplication::OnMouseButtonPressed(GNXEngine::MouseButtonPressedEvent& event)
{
    const DragMode mode = GetDragModeForButton(event.GetMouseButton());
    if (mode == DragMode::None)
    {
        return false;
    }

    // 记录拖拽起点，后续增量由 UpdateDragInteraction 轮询得到；
    // 同一时刻只保留一个拖拽模式（后按下的键生效）
    const mathutil::Vector2f position = GNXEngine::Input::GetMousePosition();
    mLastMouseX = position.x;
    mLastMouseY = position.y;

    if (mDragMode == DragMode::None && mRenderer)
    {
        mRenderer->LogCameraState("拖拽开始");
    }
    mDragMode = mode;
    return true;
}

bool MapApplication::OnMouseButtonReleased(GNXEngine::MouseButtonReleasedEvent& event)
{
    if (mDragMode == DragMode::None || GetDragModeForButton(event.GetMouseButton()) != mDragMode)
    {
        return false;
    }

    // 正常路径的即时响应；释放事件被 UI 吞掉时由 UpdateDragInteraction 兜底
    EndDrag("拖拽结束");
    return true;
}

bool MapApplication::OnMouseScrolled(GNXEngine::MouseScrolledEvent& event)
{
    if (mRenderer)
    {
        mRenderer->Zoom(static_cast<double>(event.GetYOffset()) * 1000.0);
    }
    return true;
}

MapApplication::DragMode MapApplication::GetDragModeForButton(GNXEngine::MouseCode button)
{
    switch (button)
    {
    case GNXEngine::ButtonLeft:
        return DragMode::Pan;
    case GNXEngine::ButtonRight:
        return DragMode::OrbitZoom;
    case GNXEngine::ButtonMiddle:
        return DragMode::Pitch;
    default:
        return DragMode::None;
    }
}

GNXEngine::MouseCode MapApplication::GetMouseButtonForDragMode(DragMode mode)
{
    switch (mode)
    {
    case DragMode::Pan:
        return GNXEngine::ButtonLeft;
    case DragMode::OrbitZoom:
        return GNXEngine::ButtonRight;
    case DragMode::Pitch:
        return GNXEngine::ButtonMiddle;
    default:
        return GNXEngine::ButtonLeft;
    }
}

void MapApplication::EndDrag(const char* reason)
{
    if (mDragMode == DragMode::None)
    {
        return;
    }

    mDragMode = DragMode::None;
    if (mRenderer)
    {
        mRenderer->LogCameraState(reason);
    }
}

void MapApplication::UpdateDragInteraction()
{
    if (!mRenderer || mDragMode == DragMode::None)
    {
        return;
    }

    // 「松开立即停止」：松开事件可能被 UI 吞掉或窗口失焦时丢失，以实时按键状态兜底
    if (!GNXEngine::Input::IsMouseButtonPressed(GetMouseButtonForDragMode(mDragMode)))
    {
        EndDrag("拖拽结束(按键已松开)");
        return;
    }

    const mathutil::Vector2f position = GNXEngine::Input::GetMousePosition();
    const float deltaX = position.x - mLastMouseX;
    const float deltaY = position.y - mLastMouseY;
    mLastMouseX = position.x;
    mLastMouseY = position.y;

    if (deltaX == 0.0f && deltaY == 0.0f)
    {
        return;
    }

    // 光标是逻辑坐标（与 ImGui DisplaySize 同空间），乘 DPI 缩放才是 SetLens 用的帧缓冲像素
    const GNXEngine::RenderWindowPtr window = GNXEngine::GetRenderWindow();
    const float dpiScale = window ? window->GetDPIScale() : 1.0f;
    const double dx = static_cast<double>(deltaX * dpiScale);
    const double dy = static_cast<double>(deltaY * dpiScale);

    switch (mDragMode)
    {
    case DragMode::Pan:
        // 位移取反作为「屏幕中心偏移」发射线，画面与光标同向移动（跟手）
        mRenderer->Pan(static_cast<float>(-dx), static_cast<float>(-dy));
        break;

    case DragMode::OrbitZoom:
        // 右键：横向改方位角（向右拖 -> 内容逆时针），纵向等比缩放（向下拖 -> 放大）
        mRenderer->OrbitByDrag(dx, dy, earthcore::CameraDragMode::RightButton);
        break;

    case DragMode::Pitch:
        // 中键：纵向改俯仰角（向下拖 -> 向地平线倾斜），横向分量被忽略
        mRenderer->OrbitByDrag(dx, dy, earthcore::CameraDragMode::MiddleButton);
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// ImGui 面板：目标点信息与视角控制
// ---------------------------------------------------------------------------
void MapApplication::BuildImGuiPanel()
{
    if (!mRenderer)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Always);

    if (ImGui::Begin("相机信息", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        double targetLongitude = 0.0, targetLatitude = 0.0, targetHeight = 0.0;
        mRenderer->GetTargetGeodeticDegrees(targetLongitude, targetLatitude, targetHeight);

        ImGui::Text("目标点经度: %.6f 度", targetLongitude);
        ImGui::Text("目标点纬度: %.6f 度", targetLatitude);
        ImGui::Text("相机距离: %.1f m", mRenderer->GetEyeDistance());
        bool lightingEnabled = mRenderer->IsEarthLightingEnabled();
        if (ImGui::Checkbox("地球光照", &lightingEnabled))
            mRenderer->SetEarthLightingEnabled(lightingEnabled);
        bool atmosphereEnabled = mRenderer->IsAtmosphereEnabled();
        if (ImGui::Checkbox("地球大气", &atmosphereEnabled))
            mRenderer->SetAtmosphereEnabled(atmosphereEnabled);
    }
    ImGui::End();

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 12.0f, displaySize.y - 12.0f),
                            ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    const ImGuiWindowFlags controlFlags = ImGuiWindowFlags_NoTitleBar |
                                          ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("视角控制", nullptr, controlFlags))
    {
        if (ImGui::Button(mAngleControlsOpen ? "收起视角设置" : "视角设置"))
        {
            mAngleControlsOpen = !mAngleControlsOpen;
        }

        if (mAngleControlsOpen)
        {
            float pitchDegrees = static_cast<float>(mRenderer->GetPitchAngleAtTargetDegrees());
            float azimuthDegrees = static_cast<float>(mRenderer->GetAzimuthAngleAtTargetDegrees());

            ImGui::Text("当前俯视角: %.3f 度", pitchDegrees);
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::SliderFloat("俯视角", &pitchDegrees, 0.0f, 90.0f, "%.3f 度"))
            {
                mRenderer->SetAzimuthPitchDegrees(azimuthDegrees, pitchDegrees);
            }

            ImGui::Text("当前方位角: %.3f 度", azimuthDegrees);
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::SliderFloat("方位角", &azimuthDegrees, 0.0f, 360.0f, "%.3f 度"))
            {
                mRenderer->SetAzimuthPitchDegrees(azimuthDegrees, pitchDegrees);
            }

            if (ImGui::Button("重置"))
            {
                mRenderer->SetAzimuthPitchDegrees(mDefaultAzimuthDegrees, mDefaultPitchDegrees);
            }
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// 自动化：等待瓦片加载完成后截图并退出（可复现的批量出图）
// ---------------------------------------------------------------------------
void MapApplication::UpdateAutomation()
{
    // 动画期间定期抓图，结束后退出。
    if (mAnim.enabled)
    {
        if (mFrameIndex == 1 || mFrameIndex % mAnim.captureEvery == 0)
        {
            char filePath[1024] = {0};
            snprintf(filePath, sizeof(filePath), "%s/anim_%05d.png",
                     mAnim.outputDir.c_str(), mFrameIndex);
            const earthcore::QuadTreeStats& qts = earthcore::GetQuadTreeStats();
            LOG_INFO("[四叉树] frame=%d splits=%llu merges=%llu created=%llu destroyed=%llu requests=%llu results=%llu empty=%llu horizonCulled=%llu",
                     mFrameIndex, qts.splits, qts.merges, qts.nodesCreated,
                     qts.nodesDestroyed, qts.requests, qts.results, qts.emptyResults, qts.horizonCulled);

            mRenderer->LogCameraState("动画抓图");
            if (mRenderer->SaveScreenshot(filePath))
            {
                ++mAnim.capturedCount;
            }
            else
            {
                mExitCode = 2;
                LOG_ERROR("相机动画抓图失败: %s", filePath);
            }
        }

        if (mFrameIndex >= mAnim.totalFrames)
        {
            LOG_INFO("相机动画完成: 共 %d 帧, 抓图 %d 张 -> %s",
                     mFrameIndex, mAnim.capturedCount, mAnim.outputDir.c_str());
            if (GNXEngine::RenderWindowPtr window = GNXEngine::GetRenderWindow())
            {
                window->RequestClose();
            }
        }
        return;
    }

    if (mScreenshotPath.empty() || mScreenshotAttempted)
    {
        return;
    }

    if (mFrameIndex < mScreenshotWaitFrames)
    {
        return;
    }

    mScreenshotAttempted = true;
    mRenderer->LogCameraState("截图");

    if (mRenderer->SaveScreenshot(mScreenshotPath))
    {
        LOG_INFO("自动化截图完成: %s", mScreenshotPath.c_str());
    }
    else
    {
        mExitCode = 2;
        LOG_ERROR("自动化截图失败: %s", mScreenshotPath.c_str());
    }

    // 截图完成即请求退出，主循环会走完引擎的正常资源释放流程
    if (GNXEngine::RenderWindowPtr window = GNXEngine::GetRenderWindow())
    {
        window->RequestClose();
    }
}
