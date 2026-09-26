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

    // 进程退出码：自动化截图失败时返回非 0，便于批处理脚本判定
    int GetExitCode() const { return mExitCode; }

private:
    // 鼠标拖拽模式：按键按下时建立，松开或按键状态失联时清除
    enum class DragMode
    {
        None,
        Pan,        // 左键：平移地球
        OrbitZoom,  // 右键：横向改方位角、纵向等比缩放
        Pitch       // 中键：纵向改俯仰角
    };

    bool OnMouseButtonPressed(GNXEngine::MouseButtonPressedEvent& event);
    bool OnMouseButtonReleased(GNXEngine::MouseButtonReleasedEvent& event);
    bool OnMouseScrolled(GNXEngine::MouseScrolledEvent& event);

    static DragMode GetDragModeForButton(GNXEngine::MouseCode button);
    static GNXEngine::MouseCode GetMouseButtonForDragMode(DragMode mode);

    // 每帧轮询鼠标增量并分发到位移/旋转/缩放；按键已松开则立即结束拖拽
    //
    // 不用 MouseMovedEvent 取增量：ImGui 在光标悬停面板时会吞掉移动事件（丢增量造成跳变），
    // 松开事件被吞还会让拖拽卡死，故状态由事件建立、增量在这里轮询。
    void UpdateDragInteraction();

    // 结束拖拽并记录一次相机状态
    void EndDrag(const char* reason);

    // 构建 ImGui 数值面板（方位角/俯仰角/距离与实时读数）
    void BuildImGuiPanel();

    // 解析启动配置（环境变量）并应用到相机
    void ApplyStartupOptions();

    // 自动化：等待若干帧让瓦片加载完成后截图并退出
    void UpdateAutomation();

    // 读取 GNX_MAP_ANIM_* 配置。
    void ApplyCameraAnimationOptions();

    // 绘制前按帧号更新相机。
    void UpdateCameraAnimation();

    std::unique_ptr<MapRenderer> mRenderer;
    float mLastMouseX = 0.0f;
    float mLastMouseY = 0.0f;
    DragMode mDragMode = DragMode::None;

    // 环境变量驱动的相机动画与抓图。
    struct CameraAnimation
    {
        std::string outputDir;          // GNX_MAP_ANIM_DIR（非空即启用）
        int totalFrames = 180;          // GNX_MAP_ANIM_FRAMES
        int captureEvery = 10;          // GNX_MAP_ANIM_CAPTURE_EVERY
        double fromDistance = 0.0;      // GNX_MAP_ANIM_FROM_DISTANCE
        double toDistance = 0.0;        // GNX_MAP_ANIM_TO_DISTANCE
        double fromAzimuth = 0.0;       // GNX_MAP_ANIM_FROM_AZIMUTH
        double toAzimuth = 0.0;         // GNX_MAP_ANIM_TO_AZIMUTH
        double fromPitch = 0.0;         // GNX_MAP_ANIM_FROM_PITCH
        double toPitch = 0.0;           // GNX_MAP_ANIM_TO_PITCH
        int capturedCount = 0;
        bool enabled = false;
    };
    CameraAnimation mAnim;

    // ---- 启动配置 / 自动化 ----
    // 拖拽灵敏度（1.0 为默认手感；负值反向；0 表示该维度不响应）
    double mAzimuthDragSensitivity = 1.0;   // GNX_MAP_AZIMUTH_SENSITIVITY
    double mPitchDragSensitivity = 1.0;     // GNX_MAP_PITCH_SENSITIVITY
    double mZoomDragSensitivity = 1.0;      // GNX_MAP_ZOOM_SENSITIVITY
    bool mPanelVisible = true;          // 面板是否显示（GNX_MAP_PANEL）
    std::string mScreenshotPath;        // GNX_MAP_SCREENSHOT：非空则开启自动化截图
    int mScreenshotWaitFrames = 120;    // GNX_MAP_SCREENSHOT_FRAMES：截图前等待的帧数
    int mFrameIndex = 0;
    bool mScreenshotAttempted = false;
    int mExitCode = 0;
};

#endif
