//
//  metalwindow.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#include "MetalWindow.h"
#include <QtCore>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QWheelEvent>
#include <MetalKit/MetalKit.h>
#include "MapRenderer.h"

// Simple private class for the purpose of moving the Objective-C
// bits out of the header.
class MetalWindowPrivate
{
public:
    id<MTLDevice> m_metalDevice;
    MapRenderer *m_renderer = nullptr;
};

MetalWindow::MetalWindow()
:d(new MetalWindowPrivate())
{
    setSurfaceType(QSurface::MetalSurface);
}

MetalWindow::~MetalWindow()
{
    delete d;
}

void MetalWindow::exposeEvent(QExposeEvent *)
{
    initMetal();

    d->m_renderer->DrawFrame();

    requestUpdate(); // request new animation frame
}

void MetalWindow::updateEvent()
{
    if (!d || !d->m_renderer)
    {
        return;
    }
    d->m_renderer->DrawFrame();
    requestUpdate();
}

bool MetalWindow::event(QEvent *ev)
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
        
        QScreen* screen = QGuiApplication::primaryScreen();
        qreal devicePixelRatio = screen->devicePixelRatio();
        //devicePixelRatio = 1;   //还需要适配高分屏幕
        
        mWidth = resizeEvent->size().width() * devicePixelRatio;
        mHeight = resizeEvent->size().height() * devicePixelRatio;
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
        
        if (pt != mMouseDown)
        {
            int xOffset = pt.x() - mMouseDown.x();
            int yOffset = pt.y() - mMouseDown.y();
        }
        
        return false;
    }
    
    // 鼠标移动
    else if (ev->type() == QEvent::MouseMove)
    {
        if (mIsDown)
        {
            QMouseEvent* mouseEvent = (QMouseEvent*)ev;
            QPoint pt = mouseEvent->pos();
            
            if (pt != mMouseDown)
            {
                int xOffset = pt.x() - mMouseDown.x();
                int yOffset = pt.y() - mMouseDown.y();
                
                mMouseDown = pt;
            }
        }
        
        return false;
    }
    
    // 鼠标滚轮
    else if (ev->type() == QEvent::Wheel)
    {
        QWheelEvent* wheelEvent = (QWheelEvent*)ev;
        
        int delta = wheelEvent->angleDelta().y();
        bool zoomIn = delta > 0;
        
        QPointF point = wheelEvent->position();
        
        return false;
    }
    
    else
    {
        return QWindow::event(ev);
    }
}

void MetalWindow::initMetal()
{
    if (d->m_metalDevice != nil)
        return;

    d->m_metalDevice = MTLCreateSystemDefaultDevice();
    if (!d->m_metalDevice)
        qFatal("Metal is not supported");

    // Extract NSView and Metal layer via winId()
    //CAMetalLayer *metalLayer = (CAMetalLayer *)((NSView *)winId()).layer;
    CAMetalLayer *metalLayer = (CAMetalLayer *)((NSView*)winId()).layer;

    // Create Renderer
    metalLayer.device = d->m_metalDevice;
    d->m_renderer = new MapRenderer(metalLayer);
    d->m_renderer->SetWindowSize(mWidth, mHeight);
}
