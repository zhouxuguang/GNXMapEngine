#ifndef GNX_MAP_ENGINE_MAP_APPLICATION_H
#define GNX_MAP_ENGINE_MAP_APPLICATION_H

#include "Runtime/GNXEngine/include/AppFrameWork.h"
#include "Runtime/GNXEngine/include/Events/MouseEvent.h"
#include <memory>
#include <string>

class MapRenderer;

class MapApplication final : public GNXEngine::AppFrameWork
{
public:
    explicit MapApplication(const GNXEngine::WindowProps& props);
    ~MapApplication() override;

    void Initlize() override;
    void Resize(uint32_t width, uint32_t height) override;
    void RenderFrame() override;
    void OnEvent(GNXEngine::Event& event) override;

private:
    bool OnMouseButtonPressed(GNXEngine::MouseButtonPressedEvent& event);
    bool OnMouseButtonReleased(GNXEngine::MouseButtonReleasedEvent& event);
    bool OnMouseMoved(GNXEngine::MouseMovedEvent& event);
    bool OnMouseScrolled(GNXEngine::MouseScrolledEvent& event);
    void PanTo(float x, float y);

    // 构建 ImGui 数值面板（方位角/俯仰角/距离与实时读数）
    void BuildImGuiPanel();

    // 解析启动配置（环境变量）并应用到相机
    void ApplyStartupOptions();

    // 自动化：等待若干帧让瓦片加载完成后截图并退出
    void UpdateAutomation();

    std::unique_ptr<MapRenderer> mRenderer;
    float mLastMouseX = 0.0f;
    float mLastMouseY = 0.0f;
    bool mDragging = false;

    // ---- 启动配置 / 自动化 ----
    bool mPanelVisible = true;          // 面板是否显示（GNX_MAP_PANEL）
    std::string mScreenshotPath;        // GNX_MAP_SCREENSHOT：非空则开启自动化截图
    int mScreenshotWaitFrames = 120;    // GNX_MAP_SCREENSHOT_FRAMES：截图前等待的帧数
    int mFrameIndex = 0;
    bool mScreenshotAttempted = false;
    int mExitCode = 0;
};

#endif
