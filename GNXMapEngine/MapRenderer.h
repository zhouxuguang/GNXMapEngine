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
#include "WebMercator.h"

using namespace RenderCore;

class MapRenderer
{
public:
    MapRenderer(CAMetalLayer *mtkLayer);
    
    ~MapRenderer()
    {
    }
    
    void DrawFrame();
    
    // 获得当前地图对应的级别
    int GetLevel();
    
    void SetWindowSize(uint32_t width, uint32_t height)
    {
        mWidth = width;
        mHeight = height;
    }
    
    void SetOrth(double left, double right, double top, double bottom)
    {
        mLeft   =   left;
        mRight  =   right;
        mTop    =   top;
        mBottom =   bottom;
        
        // 再根据当前级别把地理范围给校正
        int level  =  GetLevel();
        double  res =  WebMercator::resolution(level);
        double  w = res * mWidth;
        double  h = res * mHeight;

        double  cX  =  (mLeft + mRight) * 0.5;
        double  cY  =  (mBottom + mTop) * 0.5;

        mLeft   =   cX - w * 0.5;
        mRight  =   cX + w * 0.5;
        mTop    =   cY + w * 0.5;
        mBottom =   cY - w * 0.5;
    }
    
private:
    RenderCore::RenderDevicePtr mRenderdevice = nullptr;
    
    // 当前地图显示的范围
    double mLeft = -20037508;
    double mRight = 20037508;
    double mBottom = -20037508;
    double mTop = 20037508;
    
    double mWidth;
    double mHeight;
    
    mathutil::Matrix4x4f mProjection;
    
    GraphicsPipelinePtr mPipeline = nullptr;
    VertexBufferPtr mVertexBuffer = nullptr;
    TextureSamplerPtr mTexSampler = nullptr;
    UniformBufferPtr mUBO = nullptr;
    
    Texture2DPtr mTexture = nullptr;
};

#endif /* MapRenderer_hpp */
