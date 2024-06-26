//
//  metalwindow.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#ifndef metalwindow_hpp
#define metalwindow_hpp

#include <QWindow>
#include <memory>

class MetalWindowPrivate;
class MetalWindow : public QWindow
{
public:
    MetalWindow();
    ~MetalWindow();
    void exposeEvent(QExposeEvent *) override;
    void updateEvent();
    bool event(QEvent *ev) override;
    void initMetal();
private:
    MetalWindowPrivate *d;
    
    uint32_t mWidth;
    uint32_t mHeight;
    
    QPoint mMouseDown;
    bool mIsDown = false;
};

#endif /* metalwindow_hpp */
