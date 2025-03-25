//
//  TiledImage.h
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/30.
//

#ifndef GNX_EARTHENGINE_CORE_TILEDIMAGE_INCLUDE_SDGDFGHFD
#define GNX_EARTHENGINE_CORE_TILEDIMAGE_INCLUDE_SDGDFGHFD

#include "ObjectBase.h"

EARTH_CORE_NAMESPACE_BEGIN

// 瓦片的图像数据
class TiledImage : public ObjectBase
{
public:
    imagecodec::VImage image;
};

using TiledImagePtr = std::shared_ptr<TiledImage>;

EARTH_CORE_NAMESPACE_END

#endif // GNX_EARTHENGINE_CORE_TILEDIMAGE_INCLUDE_SDGDFGHFD