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
};

#endif /* metalwindow_hpp */
