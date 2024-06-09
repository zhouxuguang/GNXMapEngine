//
//  MapRenderer.hpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#ifndef MapRenderer_hpp
#define MapRenderer_hpp

#include <MetalKit/MetalKit.h>
#include "RenderCore/RenderDevice.h"
#include "MathUtil/Matrix4x4.h"

class MapRenderer
{
public:
    MapRenderer(CAMetalLayer *mtkLayer);
    
    ~MapRenderer()
    {
    }
    
    void DrawFrame();
    
private:
    RenderCore::RenderDevicePtr mRenderdevice;
    
    // 当前地图显示的范围
    double mLeft = -20037508;
    double mRight = 20037508;
    double mBottom = -20037508;
    double mTop = 20037508;
    
    mathutil::Matrix4x4f mProjection;
};

#endif /* MapRenderer_hpp */
