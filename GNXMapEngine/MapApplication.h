#ifndef GNX_MAP_ENGINE_MAP_APPLICATION_H
#define GNX_MAP_ENGINE_MAP_APPLICATION_H

#include "Runtime/GNXEngine/include/AppFrameWork.h"
#include "Runtime/GNXEngine/include/Events/MouseEvent.h"
#include <memory>

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

    std::unique_ptr<MapRenderer> mRenderer;
    float mLastMouseX = 0.0f;
    float mLastMouseY = 0.0f;
    bool mDragging = false;
};

#endif
