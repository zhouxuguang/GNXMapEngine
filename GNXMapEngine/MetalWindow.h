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
    
    QPoint mMouseDown;    //记录鼠标按下去时候的点，以及按下鼠标时，实时移动的点
    bool mIsDown = false;
    
    // 屏幕的像素比例
    qreal mDevicePixelRatio = 1.0;
};

#endif /* metalwindow_hpp */
