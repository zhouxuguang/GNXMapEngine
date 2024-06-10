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
#include "BaseLib/ThreadPool.h"
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

class MapRenderer;

class TileLoadTask : public baselib::TaskRunner
{
public:
    TileLoadTask()
    {
    }
    
    ~TileLoadTask()
    {
    }
    
    virtual void Run();
    
    MapRenderer* mRender;
    TileKey tileKey;
};

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
        
        mRenderdevice->resize(width * 2, height * 2);
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
        mTop    =   cY + h * 0.5;
        mBottom =   cY - h * 0.5;
        
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
    
    void Zoom(bool zoomIn)
    {
        int level = GetLevel();
        if (zoomIn)
        {
            level += 1;
        }
        else
        {
            level -= 1;
        }
        
        // 重新计算当前的地理范围
        double  res =  WebMercator::resolution(level);
        double  w = res * mWidth;
        double  h = res * mHeight;

        double  cX  =  (mLeft + mRight) * 0.5;
        double  cY  =  (mBottom + mTop) * 0.5;

        mLeft   =   cX - w * 0.5;
        mRight  =   cX + w * 0.5;
        mTop    =   cY + h * 0.5;
        mBottom =   cY - h * 0.5;
        
        mProjection = Matrix4x4f::CreateOrthographic(mLeft, mRight, mBottom, mTop, -100.0f, 100.0f);
    }
    
    Vector2f ScreenToWorld(const Vector2f& screenPoint)
    {
        double w = (mRight - mLeft) / mWidth;
        double h = (mTop - mBottom) / mHeight;

        double x = w * screenPoint.x + mLeft;
        double y = mTop - h * screenPoint.y;

        return  Vector2f(x, y);
    }
    
    // 鼠标定点缩放
    void ZoomByPoint(bool zoomIn, const Vector2f& screenPoint)
    {
        int level = GetLevel();
        if (zoomIn)
        {
            level += 1;
        }
        else
        {
            level -= 1;
        }
        
        level = std::max(0, level);
        
        Vector2f worldPos = ScreenToWorld(screenPoint);
        
        // 重新计算当前的地理范围
        double  res =  WebMercator::resolution(level);
        double  w = res * mWidth;
        double  h = res * mHeight;
        
        double  px  = double(screenPoint.x) / mWidth;
        double  py  = double(screenPoint.y) / mHeight;
        
        mLeft = worldPos.x - px * w;
        mRight = worldPos.x + (1 - px) * w;
        mTop =  worldPos.y + py * h;
        mBottom = worldPos.y - (1 - py) * h;
        
        mProjection = Matrix4x4f::CreateOrthographic(mLeft, mRight, mBottom, mTop, -100.0f, 100.0f);
    }
    
    // 请求瓦片
    void RequestTiles();
    
    TileDataPtr GetTileDataFromCache(const TileKey &tileKey)
    {
        TileDataPtr dataPtr = nullptr;
        mTileCacheLock.lock();
        dataPtr = mTileCache.Get(tileKey);
        mTileCacheLock.unlock();
        
        return dataPtr;
    }
    
    void PutTileDataToCache(const TileKey &tileKey, TileDataPtr tileData)
    {
        mTileCacheLock.lock();
        mTileCache.Put(tileKey, tileData);
        mTileCacheLock.unlock();
    }
    
    void PushTileData(TileDataPtr tileData)
    {
        mTileDataLock.lock();
        mTileDatas.push_back(tileData);
        mTileDataLock.unlock();
    }
    
public:
    RenderCore::RenderDevicePtr mRenderdevice = nullptr;
    
    // 当前地图显示的范围
    double mLeft = -20037508;
    double mRight = 20037508;
    double mBottom = -20037508;
    double mTop = 20037508;
    
    std::mutex mTileDataLock;
    TileDataArray mTileDatas;
    
    std::mutex mTileCacheLock;
    baselib::LruCache<TileKey, TileDataPtr> mTileCache;
    
    baselib::ThreadPool mTileLoadPool;
    
    double mWidth;
    double mHeight;
    
    Matrix4x4f mProjection;
    
    GraphicsPipelinePtr mPipeline = nullptr;
    TextureSamplerPtr mTexSampler = nullptr;
    UniformBufferPtr mUBO = nullptr;
};

#endif /* MapRenderer_hpp */
