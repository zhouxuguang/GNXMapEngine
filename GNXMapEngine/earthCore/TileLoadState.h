//
//  TileLoadState.h
//  GNXMapEngine
//
//  瓦片后台加载结果在「加载线程」与「渲染线程」之间传递的载体。
//
//  设计原因：
//  1) 后台线程（EarthNode 的瓦片线程池）不允许触碰 QuadNode —— 节点可能在加载
//     过程中被四叉树释放（zoom out 时 mChildNodes[i] = nullptr），直接写
//     node->mTexture 会造成 use-after-free / 堆损坏。
//  2) 后台线程不允许调用任何 Vulkan 接口（创建纹理、提交命令缓冲区）——
//     VkQueue 的宿主访问必须外部同步，与渲染线程并发提交属于未定义行为，
//     驱动侧通常表现为 VK_ERROR_DEVICE_LOST。
//
//  因此后台线程只往这里写「纯 CPU 数据」，GPU 资源一律由渲染线程在
//  QuadNode::Update 中创建。
//

#ifndef GNX_MAP_ENGINE_TILE_LOAD_STATE_INCLUDE_JHGDKSFG
#define GNX_MAP_ENGINE_TILE_LOAD_STATE_INCLUDE_JHGDKSFG

#include "ObjectBase.h"

#include <deque>
#include <memory>

EARTH_CORE_NAMESPACE_BEGIN

class TileLoadState
{
public:
    // 后台线程调用：标记瓦片数据已经解码完成
    void SetLoadedData(const ObjectBasePtr& data, bool isTerrain)
    {
        baselib::AutoLock lockGuard(mLock);
        mResults.push_back({data, isTerrain});
    }

    // 渲染线程调用：取出数据（只取一次，取走后再调用返回空指针）
    ObjectBasePtr TakeLoadedData(bool& isTerrain)
    {
        baselib::AutoLock lockGuard(mLock);
        if (mResults.empty())
        {
            return nullptr;
        }

        Result result = std::move(mResults.front());
        mResults.pop_front();
        isTerrain = result.isTerrain;
        return std::move(result.data);
    }

private:
    baselib::MutexLock mLock;
    struct Result
    {
        ObjectBasePtr data;
        bool isTerrain;
    };
    std::deque<Result> mResults;
};

using TileLoadStatePtr = std::shared_ptr<TileLoadState>;

EARTH_CORE_NAMESPACE_END

#endif // GNX_MAP_ENGINE_TILE_LOAD_STATE_INCLUDE_JHGDKSFG
