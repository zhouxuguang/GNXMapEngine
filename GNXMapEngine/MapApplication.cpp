#include "MapApplication.h"
#include "MapRenderer.h"
#include "Runtime/GNXEngine/include/Input.h"
#include "Runtime/GNXEngine/include/RenderWindow.h"
#include "Runtime/BaseLib/include/LogService.h"

#include <imgui.h>

#include <cstdlib>
#include <cmath>
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

    // 自动化截图
    GetEnvString("GNX_MAP_SCREENSHOT", mScreenshotPath);
    GetEnvInt("GNX_MAP_SCREENSHOT_FRAMES", mScreenshotWaitFrames);
    if (mScreenshotWaitFrames < 1)
    {
        mScreenshotWaitFrames = 1;
    }

    LOG_INFO("启动配置: 面板=%s 方位角=%.6f 度 俯仰角=%.6f 度 距离=%.3f m 截图=%s 等待帧数=%d",
             mPanelVisible ? "开" : "关",
             mRenderer->GetAzimuthAngleAtTargetDegrees(),
             mRenderer->GetPitchAngleAtTargetDegrees(),
             mRenderer->GetEyeDistance(),
             mScreenshotPath.empty() ? "(无)" : mScreenshotPath.c_str(),
             mScreenshotWaitFrames);

    mRenderer->LogCameraState("启动");
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
    dispatcher.Dispatch<GNXEngine::MouseMovedEvent>(
        [this](GNXEngine::MouseMovedEvent& e) { return OnMouseMoved(e); });
    dispatcher.Dispatch<GNXEngine::MouseScrolledEvent>(
        [this](GNXEngine::MouseScrolledEvent& e) { return OnMouseScrolled(e); });
}

bool MapApplication::OnMouseButtonPressed(GNXEngine::MouseButtonPressedEvent& event)
{
    if (event.GetMouseButton() != GNXEngine::ButtonLeft)
    {
        return false;
    }

    const mathutil::Vector2f position = GNXEngine::Input::GetMousePosition();
    mLastMouseX = position.x;
    mLastMouseY = position.y;
    mDragging = true;
    return true;
}

bool MapApplication::OnMouseButtonReleased(GNXEngine::MouseButtonReleasedEvent& event)
{
    if (event.GetMouseButton() != GNXEngine::ButtonLeft)
    {
        return false;
    }

    if (mDragging)
    {
        const mathutil::Vector2f position = GNXEngine::Input::GetMousePosition();
        PanTo(position.x, position.y);
    }
    mDragging = false;
    return true;
}

bool MapApplication::OnMouseMoved(GNXEngine::MouseMovedEvent& event)
{
    if (mDragging)
    {
        PanTo(event.GetX(), event.GetY());
    }
    return mDragging;
}

bool MapApplication::OnMouseScrolled(GNXEngine::MouseScrolledEvent& event)
{
    if (mRenderer)
    {
        mRenderer->Zoom(static_cast<double>(event.GetYOffset()) * 1000.0);
    }
    return true;
}

void MapApplication::PanTo(float x, float y)
{
    const GNXEngine::RenderWindowPtr window = GNXEngine::GetRenderWindow();
    const float dpiScale = window ? window->GetDPIScale() : 1.0f;

    if (mRenderer)
    {
        mRenderer->Pan((mLastMouseX - x) * dpiScale,
                       (mLastMouseY - y) * dpiScale);
    }

    mLastMouseX = x;
    mLastMouseY = y;
}

// ---------------------------------------------------------------------------
// ImGui 数值面板：方位角 / 俯仰角 / 距离 的设定与实时读数
// ---------------------------------------------------------------------------
void MapApplication::BuildImGuiPanel()
{
    if (!mRenderer)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430.0f, 430.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("地球相机：方位角 / 俯仰角", &mPanelVisible))
    {
        ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
        ImGui::Separator();

        // ---- 目标点处轨道方位角（0~360，正北为 0、顺时针为正）----
        float azimuthDegrees = static_cast<float>(mRenderer->GetAzimuthAngleAtTargetDegrees());
        if (ImGui::SliderFloat("轨道方位角 (度)", &azimuthDegrees, 0.0f, 360.0f, "%.3f"))
        {
            mRenderer->SetAzimuthPitchDegrees(azimuthDegrees,
                                              mRenderer->GetPitchAngleAtTargetDegrees());
        }

        // ---- 目标点处轨道俯仰角（0~90，垂直下视为 0）----
        float pitchDegrees = static_cast<float>(mRenderer->GetPitchAngleAtTargetDegrees());
        if (ImGui::SliderFloat("轨道俯仰角 (度)", &pitchDegrees, 0.0f, 90.0f, "%.3f"))
        {
            mRenderer->SetAzimuthPitchDegrees(mRenderer->GetAzimuthAngleAtTargetDegrees(),
                                              pitchDegrees);
        }

        // ---- 视线距离 ----
        float eyeDistance = static_cast<float>(mRenderer->GetEyeDistance());
        if (ImGui::DragFloat("视线距离 (m)", &eyeDistance, 1000.0f, 20.0f, 1.0e8f, "%.1f",
                             ImGuiSliderFlags_Logarithmic))
        {
            mRenderer->SetEyeDistance(static_cast<double>(eyeDistance));
        }

        ImGui::Separator();
        ImGui::Text("实测角度（两套基准）");
        ImGui::Text("轨道命令值: 方位角 %.6f 度 / 俯仰角 %.6f 度",
                    mRenderer->GetAzimuthAngleAtTargetDegrees(),
                    mRenderer->GetPitchAngleAtTargetDegrees());
        ImGui::Text("视点处  : 方位角 %.6f 度 / 俯仰角 %.6f 度",
                    mRenderer->GetAzimuthAngleDegrees(),
                    mRenderer->GetPitchAngleDegrees());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("视点处量测严格按需求定义（用视点的大地法线）；\n"
                              "轨道命令值以目标点 ENU 为基准，与上面的滑条一致。\n"
                              "椭球曲率导致两者存在系统性差异（高空尤为明显）。");
        }

        ImGui::Separator();
        double eyeLongitude = 0.0, eyeLatitude = 0.0, eyeHeight = 0.0;
        double targetLongitude = 0.0, targetLatitude = 0.0, targetHeight = 0.0;
        mRenderer->GetEyeGeodeticDegrees(eyeLongitude, eyeLatitude, eyeHeight);
        mRenderer->GetTargetGeodeticDegrees(targetLongitude, targetLatitude, targetHeight);

        ImGui::Text("视点  : 经 %.6f 度 纬 %.6f 度 高 %.1f m", eyeLongitude, eyeLatitude, eyeHeight);
        ImGui::Text("目标点: 经 %.6f 度 纬 %.6f 度 高 %.1f m",
                    targetLongitude, targetLatitude, targetHeight);

        const Vector3d viewInEun = mRenderer->GetViewDirectionInEyeEun();
        ImGui::Text("视线(东/北/天): %.6f / %.6f / %.6f", viewInEun.x, viewInEun.y, viewInEun.z);

        ImGui::Separator();
        ImGui::TextDisabled("滑块为目标点 ENU 轨道角：正北=0、顺时针为正，垂直下视=0");
        ImGui::TextDisabled("视点处实测角使用视点大地法线，受地球曲率影响与轨道角可不同");
        ImGui::TextDisabled("拖拽滑条时目标点与视线距离保持不变，相机绕目标点旋转");
        ImGui::TextDisabled("鼠标左键拖拽=平移, 滚轮=缩放");
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// 自动化：等待瓦片加载完成后截图并退出（可复现的批量出图）
// ---------------------------------------------------------------------------
void MapApplication::UpdateAutomation()
{
    ++mFrameIndex;

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
