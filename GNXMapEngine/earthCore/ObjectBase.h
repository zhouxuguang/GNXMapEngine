//
//  ObjectBase.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_EARTHENGINE_CORE_OBJECTBASE_INCLUDE_DKGHFGHFG
#define GNX_EARTHENGINE_CORE_OBJECTBASE_INCLUDE_DKGHFGHFG

#include "EarthEngineDefine.h"

EARTH_CORE_NAMESPACE_BEGIN

/**
 * 只能使用智能指针的基类，继承了这个类的对象，只能用std::make_shared创建智能指针对象，并且继承方式只能是public
 */
class ObjectBase : public std::enable_shared_from_this<ObjectBase>
{
public:
    using ObjectBasePtr = std::shared_ptr<ObjectBase>;
public:
    ObjectBase()
    {}

    virtual ~ObjectBase()
    {}

    ObjectBasePtr ptr()
    {
        return shared_from_this();
    }
    
    template<class T>   
    std::shared_ptr<T> toPtr()
    {
        return std::dynamic_pointer_cast<T>(ptr());
    }
};

using ObjectBasePtr = std::shared_ptr<ObjectBase>;

EARTH_CORE_NAMESPACE_END

#endif // GNX_EARTHENGINE_CORE_OBJECTBASE_INCLUDE_DKGHFGHFG