//
//  VulkanWindow.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#include "VulkanWindow.h"
#include <QtCore>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QWheelEvent>
#include "MapRenderer.h"

// Simple private class for the purpose of moving the Objective-C
// bits out of the header.
class VulkanWindowPrivate
{
public:
    MapRenderer *m_renderer = nullptr;
};

VulkanWindow::VulkanWindow()
:d(new VulkanWindowPrivate())
{
    setSurfaceType(QSurface::VulkanSurface);
    
    QScreen* screen = QGuiApplication::primaryScreen();
    mDevicePixelRatio = screen->devicePixelRatio();
}

VulkanWindow::~VulkanWindow()
{
    delete d;
}

void VulkanWindow::exposeEvent(QExposeEvent *)
{
    initVulkan();

    d->m_renderer->DrawFrame();

    requestUpdate(); // request new animation frame
}

void VulkanWindow::updateEvent()
{
    if (!d || !d->m_renderer)
    {
        return;
    }
    d->m_renderer->DrawFrame();
    requestUpdate();
}

bool VulkanWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::UpdateRequest)
    {
        updateEvent();
        return false;
    } 
    else if (ev->type() == QEvent::Resize)
    {
        QResizeEvent* resizeEvent = (QResizeEvent*)ev;
        printf("resize = width = %d, height = %d\n", resizeEvent->size().width(), resizeEvent->size().height());
        
        mWidth = resizeEvent->size().width() * mDevicePixelRatio;
        mHeight = resizeEvent->size().height() * mDevicePixelRatio;
        updateEvent();
        
        if (d->m_renderer)
        {
            d->m_renderer->SetWindowSize(mWidth, mHeight);
        }
        
        return false;
    }
    
    // 鼠标按下
    else if (ev->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = (QMouseEvent*)ev;
        mMouseDown = mouseEvent->pos();
        mIsDown = true;
        return false;
    }
    
    // 鼠标释放
    else if (ev->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* mouseEvent = (QMouseEvent*)ev;
        mIsDown = false;
        QPoint pt = mouseEvent->pos();
        
        if (d->m_renderer)
        {
            QPointF offset = QPointF(mMouseDown - pt);
            printf("pan offset x = %f, y = %f\n", offset.x(), offset.y());
            
            d->m_renderer->Pan(offset.x() * mDevicePixelRatio, offset.y() * mDevicePixelRatio);
        }
        mMouseDown = QPoint(0, 0);
        
        return false;
    }
    
    // 鼠标移动
    else if (ev->type() == QEvent::MouseMove)
    {
        if (mIsDown)
        {
            QMouseEvent* mouseEvent = (QMouseEvent*)ev;
            QPoint pt = mouseEvent->pos();
            
            if (d->m_renderer)
            {
                QPointF offset = QPointF(mMouseDown - pt);
                printf("pan offset x = %f, y = %f\n", offset.x(), offset.y());
                
                d->m_renderer->Pan(offset.x() * mDevicePixelRatio, offset.y() * mDevicePixelRatio);
            }
            
            mMouseDown = pt;
        }
        
        return false;
    }
    
    // 鼠标滚轮
    else if (ev->type() == QEvent::Wheel)
    {
        QWheelEvent* wheelEvent = (QWheelEvent*)ev;
        
        int delta = wheelEvent->angleDelta().y();
        bool zoomIn = delta > 0;
        
        if (d->m_renderer)
        {
            double deltaDistance = delta * 1000;
            d->m_renderer->Zoom(deltaDistance);
        }
        
        QPointF point = wheelEvent->position();
        
        return false;
    }
    
    else
    {
        return QWindow::event(ev);
    }
}

void VulkanWindow::initVulkan()
{
    // Create Renderer
    d->m_renderer = new MapRenderer((HWND)winId());
    d->m_renderer->SetWindowSize(mWidth, mHeight);
}
