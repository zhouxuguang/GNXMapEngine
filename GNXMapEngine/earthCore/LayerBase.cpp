#include "LayerBase.h"

EARTH_CORE_NAMESPACE_BEGIN

LayerBase::LayerBase(const std::string& name, LayerType type)
{
    mName = name;
    mLayerType = type;
}

LayerBase::~LayerBase()
{
}

// 创建瓦片加载的任务
void LayerBase::createTask()
{
}

EARTH_CORE_NAMESPACE_END