//
//  MapRenderer.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#ifndef MapRenderer_hpp
#define MapRenderer_hpp

#include "RenderCore/RenderDevice.h"
#include "earthCore/EarthEngineDefine.h"
#include "earthCore/EarthCamera.h"
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
    
    void SetWindowSize(uint32_t width, uint32_t height);
    
    void Zoom(double deltaDistance);
    
    void Pan(float offsetX, float offsetY);
    
    void TestAtmo();
    
private:
    RenderCore::RenderDevicePtr mRenderdevice = nullptr;
    SceneManager* mSceneManager;
    
    double mWidth;
    double mHeight;
    
    uint64_t mLastTime = 0;
    
    earthcore::EarthCameraPtr mCameraPtr = nullptr;
    
    void BuildEarthNode();
    
    RenderCore::RenderTexturePtr transmittance_texture = nullptr;
    RenderCore::GraphicsPipelinePtr mPipeline = nullptr;   //渲染管线
    RenderCore::UniformBufferPtr mUBO = nullptr;
};

#endif /* MapRenderer_hpp */
