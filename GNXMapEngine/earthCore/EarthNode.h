//
//  EarthNode.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_EARTHENGINE_CORE_EARTHNODE_INCLUDE_JJHSGH
#define GNX_EARTHENGINE_CORE_EARTHNODE_INCLUDE_JJHSGH

#include "EarthEngineDefine.h"
#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

// 地球的场景节点
class EarthNode : public SceneNode
{
public:
    EarthNode(const Ellipsoid& ellipsoid);
    
    ~EarthNode();
    
private:
    const Ellipsoid& mEllipsoid;
};

EARTH_CORE_NAMESPACE_END

#endif /* GNX_EARTHENGINE_CORE_EARTHNODE_INCLUDE_JJHSGH */
