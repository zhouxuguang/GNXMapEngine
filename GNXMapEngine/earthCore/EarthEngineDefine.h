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
#include "Runtime/MathUtil/include/MathUtil.h"
#include "Runtime/MathUtil/include/Vector3.h"
#include "Runtime/MathUtil/include/Vector2.h"
#include "Runtime/MathUtil/include/Matrix4x4.h"
#include "Runtime/RenderSystem/include/SceneManager.h"
#include "Runtime/RenderSystem/include/mesh/Mesh.h"
#include "Runtime/RenderSystem/include/Camera.h"
#include "Runtime/RenderSystem/include/Ray.h"
#include "Runtime/RenderSystem/include/Plane.h"
#include "Runtime/RenderSystem/include/AABB.h"
#include "Runtime/RenderSystem/include/OBB.h"
#include "Runtime/RenderSystem/include/Frustum.h"
#include "Runtime/ImageCodec/include/ImageDecoder.h"
#include "Runtime/BaseLib/include/BaseLib.h"
#include "Runtime/RenderCore/include/RCTexture.h"

USING_NS_MATHUTIL
USING_NS_RENDERSYSTEM

static const double WGS_84_RADIUS_EQUATOR = 6378140;

#define EARTH_CORE_NAMESPACE_BEGIN namespace earthcore {
#define EARTH_CORE_NAMESPACE_END }

#define EARTH_CORE_EXPORT


#endif /* REATH_ENGINE_CORE_DEFINE */
