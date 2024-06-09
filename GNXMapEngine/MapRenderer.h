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
};

#endif /* MapRenderer_hpp */
