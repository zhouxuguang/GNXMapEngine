//
//  MapRenderer.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#ifndef MapRenderer_hpp
#define MapRenderer_hpp

#include "RenderCore/RenderDevice.h"
#include "MathUtil/Matrix4x4.h"
#include "BaseLib/LruCache.h"
#include "WebMercator.h"

using namespace RenderCore;

class TileData
{
public:
    Vector2i key;       //xy方向编号
    Vector2d     start;  //起始点
    Vector2d     end;    //结束点
    Texture2DPtr texture;
    VertexBufferPtr vertexBuffer;
};

typedef std::shared_ptr<TileData> TileDataPtr;

struct TileKey
{
    int x;
    int y;
    int level;
    
    bool operator == (const TileKey& other) const
    {
        return x == other.x && y == other.y && level == other.level;
    }
};

namespace std 
{
    template <> struct hash<TileKey>
    {
        size_t operator()(const TileKey& p) const
        {
            auto hash1 = std::hash<int>{}(p.x);
            auto hash2 = std::hash<int>{}(p.y);
            auto hash3 = std::hash<int>{}(p.level);
            return hash1 ^ (hash2 << 1) ^ (hash3 << 2);
        }
    };
}

typedef std::vector<TileDataPtr> TileDataArray;

class MapRenderer
{
public:
    MapRenderer(void *mtkLayer);
    
    ~MapRenderer()
    {
    }
    
    void DrawFrame();
    
    void DrawTile(RenderEncoderPtr renderEncoder, const TileData& tileData);
    
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
        
        mProjection = Matrix4x4f::CreateOrthographic(mLeft, mRight, mBottom, mTop, -100.0f, 100.0f);
    }
    
    void Offset(int xOffset, int yOffset)
    {
        double xRes = (mRight - mLeft) / mWidth;
        double yRes = (mTop - mBottom) / mHeight;

        mLeft    -=  xOffset * xRes;
        mRight   -=  xOffset * xRes;

        mTop     +=  yOffset * xRes;
        mBottom  +=  yOffset * xRes;
        
        mProjection = Matrix4x4f::CreateOrthographic(mLeft, mRight, mBottom, mTop, -100.0f, 100.0f);
    }
    
    // 请求瓦片
    void RequestTiles();
    
private:
    RenderCore::RenderDevicePtr mRenderdevice = nullptr;
    
    // 当前地图显示的范围
    double mLeft = -20037508;
    double mRight = 20037508;
    double mBottom = -20037508;
    double mTop = 20037508;
    
    TileDataArray mTileDatas;
    
    baselib::LruCache<TileKey, TileDataPtr> mTileCache;
    
    double mWidth;
    double mHeight;
    
    Matrix4x4f mProjection;
    
    GraphicsPipelinePtr mPipeline = nullptr;
    TextureSamplerPtr mTexSampler = nullptr;
    UniformBufferPtr mUBO = nullptr;
};

#endif /* MapRenderer_hpp */
