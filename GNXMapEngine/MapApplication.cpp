#include "MapApplication.h"
#include "MapRenderer.h"
#include "Runtime/GNXEngine/include/Input.h"
#include "Runtime/GNXEngine/include/RenderWindow.h"

MapApplication::MapApplication(const GNXEngine::WindowProps& props)
    : AppFrameWork(props)
{
}

MapApplication::~MapApplication() = default;

void MapApplication::Initlize()
{
    AppFrameWork::Initlize();
    mRenderer = std::make_unique<MapRenderer>();
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
    if (mRenderer)
    {
        mRenderer->DrawFrame();
    }
}

void MapApplication::OnEvent(GNXEngine::Event& event)
{
    GNXEngine::EventDispatcher dispatcher(event);
    dispatcher.Dispatch<GNXEngine::MouseButtonPressedEvent>(
        [this](GNXEngine::MouseButtonPressedEvent& e) { return OnMouseButtonPressed(e); });
    dispatcher.Dispatch<GNXEngine::MouseButtonReleasedEvent>(
        [this](GNXEngine::MouseButtonReleasedEvent& e) { return OnMouseButtonReleased(e); });
    dispatcher.Dispatch<GNXEngine::MouseMovedEvent>(
        [this](GNXEngine::MouseMovedEvent& e) { return OnMouseMoved(e); });
    dispatcher.Dispatch<GNXEngine::MouseScrolledEvent>(
        [this](GNXEngine::MouseScrolledEvent& e) { return OnMouseScrolled(e); });

    AppFrameWork::OnEvent(event);
}

bool MapApplication::OnMouseButtonPressed(GNXEngine::MouseButtonPressedEvent&)
{
    const mathutil::Vector2f position = GNXEngine::Input::GetMousePosition();
    mLastMouseX = position.x;
    mLastMouseY = position.y;
    mDragging = true;
    return true;
}

bool MapApplication::OnMouseButtonReleased(GNXEngine::MouseButtonReleasedEvent&)
{
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
