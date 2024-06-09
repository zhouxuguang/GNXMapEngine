//
//  metalwindow.cpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#include "metalwindow.h"
#include <QtCore>
#include <QResizeEvent>
#include <QMouseEvent>
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
        mWidth = resizeEvent->size().width();
        mHeight = resizeEvent->size().height();
        updateEvent();
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
            
            d->m_renderer->Offset(xOffset, yOffset);
            d->m_renderer->RequestTiles();

        }
        
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
    d->m_renderer->SetOrth(-20037508, 20037508, 20037508, -20037508);
    d->m_renderer->RequestTiles();
}
