//
//  EarthNode.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_EARTHENGINE_CORE_EARTHNODE_INCLUDE_JJHSGH
#define GNX_EARTHENGINE_CORE_EARTHNODE_INCLUDE_JJHSGH

#include "Ellipsoid.h"
#include "QuadTree.h"
#include "LayerBase.h"

EARTH_CORE_NAMESPACE_BEGIN

// 地球的场景节点
class EarthNode : public SceneNode
{
public:
    EarthNode(const Ellipsoid& ellipsoid, EarthCameraPtr cameraPtr);
    
    ~EarthNode();
    
    const Ellipsoid& GetEllipsoid() const
    {
        return mEllipsoid;
    }

    virtual void Update(float deltaTime) override;

    void Initialize();

    /**
     * 请求瓦片
     */
    void RequestTile(QuadNode* node);

    /**
     * 取消瓦片请求
     */
    void CancelRequest(QuadNode* node);

    void AddLayer(LayerBasePtr layer)
    {
        mLayers.push_back(layer);
    }
    
private:
    const Ellipsoid& mEllipsoid;
    std::vector<QuadTreePtr> mQuadNodes;   //四叉树根节点，wgs84的话就有两个
    EarthCameraPtr mCameraPtr = nullptr;
    baselib::ThreadPool mTileLoadPool;    // 瓦片数据加载线程池
    std::vector<LayerBasePtr> mLayers;   //图层列表
    bool mInited = false;
};

EARTH_CORE_NAMESPACE_END

#endif /* GNX_EARTHENGINE_CORE_EARTHNODE_INCLUDE_JJHSGH */
