#include "QuadTree.h"
#include "BoundingRegion.h"
#include "EarthNode.h"
#include "TiledImage.h"
#include "Runtime/RenderSystem/include/RenderParameter.h"
#include "Runtime/RenderSystem/include/ImageTextureUtil.h"
#include "Runtime/BaseLib/include/LogService.h"

#include <vector>

EARTH_CORE_NAMESPACE_BEGIN

/**
 把解码好的瓦片图像创建成 GPU 纹理。

 通过渲染后端的异步接口提交上传，并把完成凭据交给瓦片节点。
 节点只在上传完成、顶点和索引缓冲都可用时参与绘制。
 */
static RenderCore::RCTexture2DPtr CreateTileTexture(const imagecodec::VImage& image,
                                                   RenderCore::TextureUploadPtr& upload)
{
	const uint32_t width = image.GetWidth();
	const uint32_t height = image.GetHeight();
	const uint8_t* data = image.GetImageData();
	uint32_t bytesPerRow = image.GetBytesPerRow();

	// Vulkan 后端不支持 24 位 RGB 采样格式（kTexFormatRGB24 无对应 VkFormat，
	// 创建纹理会得到 VK_FORMAT_UNDEFINED 并导致失败）。这里先转成 32 位格式。
	std::vector<uint8_t> converted;
	RenderCore::TextureFormat textureFormat = RenderCore::kTexFormatInvalid;
	const imagecodec::ImagePixelFormat format = image.GetFormat();
	const bool isSRGB = (format == imagecodec::FORMAT_SRGB8 || format == imagecodec::FORMAT_SRGB8_ALPHA8);

	if (format == imagecodec::FORMAT_RGB8 || format == imagecodec::FORMAT_SRGB8)
	{
		const uint32_t dstBytesPerRow = width * 4;
		converted.resize((size_t)dstBytesPerRow * height);

		for (uint32_t y = 0; y < height; ++y)
		{
			const uint8_t* srcRow = data + (size_t)y * bytesPerRow;
			uint8_t* dstRow = converted.data() + (size_t)y * dstBytesPerRow;
			for (uint32_t x = 0; x < width; ++x)
			{
				dstRow[x * 4 + 0] = srcRow[x * 3 + 0];
				dstRow[x * 4 + 1] = srcRow[x * 3 + 1];
				dstRow[x * 4 + 2] = srcRow[x * 3 + 2];
				dstRow[x * 4 + 3] = 255;
			}
		}

		data = converted.data();
		bytesPerRow = dstBytesPerRow;
		textureFormat = isSRGB ? RenderCore::kTexFormatSRGB8_ALPHA8 : RenderCore::kTexFormatRGBA8;
	}
	else
	{
		textureFormat = RenderSystem::ImageTextureUtil::getTextureDescriptor(image).format;
	}

	if (width == 0 || height == 0 || data == nullptr || textureFormat == RenderCore::kTexFormatInvalid)
	{
		LOG_ERROR("CreateTileTexture: invalid image (size=%ux%u, format=%d)", width, height, (int)format);
		return nullptr;
	}

	RenderCore::RenderDevicePtr renderDevice = GetRenderDevice();
	if (!renderDevice)
	{
		return nullptr;
	}

	RenderCore::RCTexture2DPtr texture = renderDevice->CreateTexture2D(textureFormat,
																	  RenderCore::TextureUsage::TextureUsageShaderRead,
																	  width, height, 1);
	if (!texture)
	{
		LOG_ERROR("CreateTileTexture: create texture failed (size=%ux%u, format=%u)", width, height, textureFormat);
		return nullptr;
	}

	RenderCore::Rect2D rect(0, 0, width, height);
	upload = texture->ReplaceRegionAsync(rect, 0, data, bytesPerRow);
	if (!upload || upload->GetStatus() == RenderCore::TextureUploadStatus::Failed)
	{
		LOG_ERROR("CreateTileTexture: asynchronous upload failed");
		return nullptr;
	}
	return texture;
}

QuadNode::QuadNode(EarthNode* earthNode, QuadNode* parent, const Vector2d& vStart, const Vector2d& vEnd, uint32_t level, ChildRegion region) : mDemData(Ellipsoid::WGS84)
{
	mEarthNode = earthNode;
	mRegion = region;
	mParent = parent;
	mLLStart = vStart;
	mLLEnd = vEnd;

	Vector2d xLLCenter = GetLonLatCenter();
	// 计算瓦片ID，并赋值
	mTileID = GetTileID(level, xLLCenter.x, xLLCenter.y);

	// 计算瓦片的世界坐标范围
	Ellipsoid wgs84 = Ellipsoid::WGS84;

	Geodetic3D llPoint1 = Geodetic3D(mLLStart.x, mLLStart.y);
	Vector3d point1 = wgs84.CartographicToCartesian(llPoint1);

	Geodetic3D llPoint2 = Geodetic3D(mLLEnd.x, mLLEnd.y);
	Vector3d point2 = wgs84.CartographicToCartesian(llPoint2);

	GlobeRectangle globeRec(mLLStart.x, mLLStart.y, mLLEnd.x, mLLEnd.y);
	BoundingRegion geoBound(globeRec, 0, 0, wgs84);
	mBoundingBox = geoBound.getBoundingBox().ToAxisAligned();

	mDemData.SetStartEndGeoCoord(vStart, vEnd);
	mDemData.FillFace();
	mDemData.FillUV(Vector2f(0.0f, 1.0f), Vector2f(1.0f, 0.0f));

	// 计算瓦片局部偏移的矩阵的uniform
	mStartPoint = wgs84.CartographicToCartesian(Geodetic3D(mLLStart.x, mLLStart.y, 0));

	cbPerObject modelMatrix;
	modelMatrix.MATRIX_M = mathutil::Matrix4x4f::CreateTranslate(mStartPoint.x, mStartPoint.y, mStartPoint.z);
	modelMatrix.MATRIX_M_INV = modelMatrix.MATRIX_M.Inverse();
	mLocalUniform = GetRenderDevice()->CreateUniformBufferWithSize(sizeof(cbPerObject));
	mLocalUniform->SetData(&modelMatrix, 0, sizeof(cbPerObject));

	mChildNodes[0] = nullptr;
	mChildNodes[1] = nullptr;
	mChildNodes[2] = nullptr;
	mChildNodes[3] = nullptr;

	// 节点创建时就准备好网格缓冲（此时按平地生成），这样影像瓦片一到达就能
	// 立即渲染，不会出现「父节点因为已经有子节点而不再绘制、子节点又还没
	// 生成顶点缓冲」导致的空洞。
	EnsureGpuBuffers();

	mEarthNode->RequestTile(this);
}

QuadNode::~QuadNode()
{
	mEarthNode->CancelRequest(this);

	for (int i = 0; i < 4; ++i)
	{
		mChildNodes[i] = nullptr;
	}
}

// 判断是否有子节点
inline bool QuadNode::HasChild() const
{
	return mChildNodes[0] != nullptr;
}

// 四叉树节点的经纬度中心点
inline Vector2d QuadNode::GetLonLatCenter() const
{
	return  (mLLStart + mLLEnd) * 0.5;
}

// 四叉树节点的经纬度范围
inline Vector2d QuadNode::GetLonLatRange() const
{
	return (mLLEnd - mLLStart);
}

void QuadNode::Update(const EarthCameraPtr& camera)
{
	if (!camera)
	{
		return;
	}

	// 后台线程加载完成的数据在这里（渲染线程）转成 GPU 资源
	ApplyLoadedTileData();

	// 保证网格缓冲已经建立：先按当前高度（没有 DEM 数据时为 0，即平地）建一份，
	// 影像瓦片到达后就能立刻渲染，不会因为等待 DEM 而出现空洞。
	EnsureGpuBuffers();
	if (IsGpuReady()) mStatusFlag |= FLAG_RENDER;
	else mStatusFlag &= ~FLAG_RENDER;

	// 判断瓦片和视锥体是否相交，相交的话去掉被裁剪的标记，否则加上被裁剪的标记
	Frustumd frustum;
	Matrix4x4d coloMatrix;
	Matrix4x4f viewProjMat = camera->GetProjectionMatrix() * camera->GetViewMatrix();
	for (uint16_t i = 0; i < 4; i ++)
	{
		for (uint16_t j = 0; j < 4; j++)
		{
			coloMatrix[i][j] = viewProjMat[i][j];
		}
	}
	frustum.InitFrustum(coloMatrix);
    
    // test
    Vector3d midPoint = Ellipsoid::WGS84.CartographicToCartesian(Geodetic3D(110, 23, 0));
    Sphered sphere(midPoint, 20);
    if (frustum.IsSphereInFrustum(sphere))
    {
        printf("");
    }

 	if (frustum.IsBoxInFrustum(mBoundingBox))
 	{
 		mStatusFlag &= ~FLAG_HAS_CULL;
 	}
 	else
 	{
 		mStatusFlag |= FLAG_HAS_CULL;
 	}

	// 相机位置
	Vector3f eyePosition = camera->GetPosition();

	// 瓦片中心点
	Vector3d vWCenter = mBoundingBox.center;
	Vector3d min = mBoundingBox.minimum;
	Vector3d max = mBoundingBox.maximum;

	Vector3d vWSize = max - min;

	double fSize = vWSize.Length() * 0.5;
	double distance = (vWCenter - Vector3d(eyePosition.x, eyePosition.y, eyePosition.z)).Length();

	if (distance / fSize < 1 && HasNoFlag(mStatusFlag, FLAG_HAS_CULL))
	{
		if (!HasChild() && HasImage(mStatusFlag))
		{
			Vector2d vLlCenter = GetLonLatCenter();
			Vector2d vLLHalf = GetLonLatRange() * 0.5;

			// 开始分裂出新的瓦片

			mChildNodes[CHILD_LT] = std::make_shared<QuadNode>(mEarthNode, this
				, Vector2d(vLlCenter.x - vLLHalf.x, vLlCenter.y)
				, Vector2d(vLlCenter.x, vLlCenter.y + vLLHalf.y)
				, mTileID.level + 1
				, CHILD_LT
			);

			mChildNodes[CHILD_RT] = std::make_shared<QuadNode>(mEarthNode, this
				, Vector2d(vLlCenter.x, vLlCenter.y)
				, Vector2d(vLlCenter.x + vLLHalf.x, vLlCenter.y + vLLHalf.y)
				, mTileID.level + 1
				, CHILD_RT
			);

			mChildNodes[CHILD_LB] = std::make_shared<QuadNode>(mEarthNode, this
				, Vector2d(vLlCenter.x - vLLHalf.x, vLlCenter.y - vLLHalf.y)
				, Vector2d(vLlCenter.x, vLlCenter.y)
				, mTileID.level + 1
				, CHILD_LB
			);

			mChildNodes[CHILD_RB] = std::make_shared<QuadNode>(mEarthNode, this
				, Vector2d(vLlCenter.x, vLlCenter.y - vLLHalf.y)
				, Vector2d(vLlCenter.x + vLLHalf.x, vLlCenter.y)
				, mTileID.level + 1
				, CHILD_RB
			);
		}
		else
		{
			for (int i = 0; i < 4; ++i)
			{
				if (mChildNodes[i] && HasNoFlag(mStatusFlag, FLAG_HAS_CULL))
				{
					mChildNodes[i]->Update(camera);
				}
				else
				{
					mStatusFlag &= FLAG_RENDER;
				}
			}
		}
	}
	else if (distance / fSize > 1.45)
	{
		for (int i = 0; i < 4; ++i)
		{
			mChildNodes[i] = nullptr;
		}
	}
}

void QuadNode::GetRenderableNodes(QuadNodeArray& nodes)
{
	if (HasChild())
	{
		mChildNodes[0]->GetRenderableNodes(nodes);
		mChildNodes[1]->GetRenderableNodes(nodes);
		mChildNodes[2]->GetRenderableNodes(nodes);
		mChildNodes[3]->GetRenderableNodes(nodes);
	}
	else
	{
		// IsGpuReady 是硬性前置条件：纹理为空时 shader 会采样到未初始化的
		// push descriptor，顶点缓冲为空时绘制会读未绑定的顶点缓冲，
		// 两者在驱动侧都可能直接导致 VK_ERROR_DEVICE_LOST。
		if (HasFlag(mStatusFlag, FLAG_RENDER) && HasNoFlag(mStatusFlag, FLAG_HAS_CULL) && IsGpuReady())
		{
			nodes.push_back(this);
		}
	}
}

bool QuadNode::IsGpuReady() const
{
	// 绘制需要同时满足：
	// 1) 纹理有效 —— 否则 gDiffuseMap 描述符从未被 push，shader 采样到未初始化的
	//    描述符（push descriptor 内容是未定义的，可能是垃圾 imageView），
	//    驱动侧读取非法描述符会直接报 VK_ERROR_DEVICE_LOST；
	// 2) 顶点/索引缓冲有效 —— 否则 vkCmdDrawIndexed 使用未绑定的顶点缓冲。
	return mTexture != nullptr && mVertexBuffer != nullptr && mIndexBuffer != nullptr &&
		mTextureUpload &&
		mTextureUpload->GetStatus() == RenderCore::TextureUploadStatus::Complete;
}

void QuadNode::EnsureGpuBuffers()
{
	if (mIndexBuffer != nullptr && mVertexBuffer != nullptr)
	{
		return;
	}

	RenderCore::RenderDevicePtr renderDevice = GetRenderDevice();
	if (!renderDevice)
	{
		return;
	}

	// 用当前高度生成顶点（DEM 尚未到达时高度为 0）
	mDemData.FillVertex();

	mIndexBuffer = renderDevice->CreateIndexBuffer(mDemData.GetFaceData(), mDemData.GetFaceBytes(), RenderCore::StorageModePrivate);
	mVertexBuffer = renderDevice->CreateVertexBuffer(mDemData.GetVertData(), mDemData.GetVertBytes(), RenderCore::StorageModePrivate);
	mInited = (mVertexBuffer != nullptr) && (mIndexBuffer != nullptr);
}

void QuadNode::ApplyLoadedTileData()
{
	bool isTerrain = false;
	ObjectBasePtr loadedData = mLoadState->TakeLoadedData(isTerrain);
	if (!loadedData)
	{
		return;
	}

	TiledImagePtr tiledImage = loadedData->toPtr<TiledImage>();
	if (!tiledImage)
	{
		return;
	}

	RenderCore::RenderDevicePtr renderDevice = GetRenderDevice();
	if (!renderDevice)
	{
		return;
	}

	if (isTerrain)
	{
		// 高度数据在渲染线程上转成顶点位置/法线：
		// 后台线程只持有解码后的数据，避免与渲染线程同时读写 mDemData
		mDemData.FillHeight(tiledImage->heightData);
		mDemData.FillVertex();
		mStatusFlag |= FLAG_HAS_DEM;

		if (mIndexBuffer == nullptr)
		{
			EnsureGpuBuffers();
		}
		else
		{
			// 用真实高度重建顶点缓冲（索引拓扑不变）。
			// 旧缓冲由垃圾收集器延迟释放，不会与在飞行的帧冲突。
			mVertexBuffer = renderDevice->CreateVertexBuffer(mDemData.GetVertData(), mDemData.GetVertBytes(), RenderCore::StorageModePrivate);
		}
	}
	else
	{
		// 纹理提交到上传队列；凭证完成前不允许绘制。
		mTexture = CreateTileTexture(tiledImage->image, mTextureUpload);
		if (mTexture)
		{
			mEarthNode->TrackTextureUpload(mTextureUpload);
			mStatusFlag |= FLAG_HAS_IMAGE;
		}
	}

	// 只有 GPU 资源全部就绪才允许进入渲染列表。
	// 之前的实现由「先到的那个图层」直接置 FLAG_RENDER，导致纹理还没加载完
	// （或 DEM 还没到）就已经提交绘制，这正是启动/缩放时 device lost 的直接原因。
	if (IsGpuReady())
	{
		mStatusFlag |= FLAG_RENDER;
	}
	else
	{
		mStatusFlag &= ~FLAG_RENDER;
	}
}

EARTH_CORE_NAMESPACE_END
