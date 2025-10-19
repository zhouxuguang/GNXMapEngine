//
//  EarthEngineDefine.h
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#ifndef REATH_ENGINE_CORE_DEFINE
#define REATH_ENGINE_CORE_DEFINE

#include <stdio.h>
#include <math.h>
#include "MathUtil/Math3DCommon.h"
#include "MathUtil/Vector3.h"
#include "MathUtil/Vector2.h"
#include "RenderSystem/SceneManager.h"
#include "RenderSystem/mesh/Mesh.h"
#include "RenderSystem/Camera.h"
#include "RenderSystem/Ray.h"
#include "RenderSystem/Plane.h"
#include "RenderSystem/AABB.h"
#include "RenderSystem/Frustum.h"
#include "ImageCodec/ImageDecoder.h"
#include "BaseLib/BaseLib.h"
#include "RenderCore/RCTexture.h"

USING_NS_MATHUTIL
USING_NS_RENDERSYSTEM

static const double WGS_84_RADIUS_EQUATOR = 6378140;

#define EARTH_CORE_NAMESPACE_BEGIN namespace earthcore {
#define EARTH_CORE_NAMESPACE_END }

#define EARTH_CORE_EXPORT


#endif /* REATH_ENGINE_CORE_DEFINE */
