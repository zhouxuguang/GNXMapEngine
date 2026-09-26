#include "QuadTree.h"
#include "BoundingRegion.h"
#include "EarthNode.h"
#include "TiledImage.h"
#include "Runtime/RenderSystem/include/RenderParameter.h"
#include "Runtime/RenderSystem/include/ImageTextureUtil.h"
#include "Runtime/BaseLib/include/LogService.h"

#include <vector>

EARTH_CORE_NAMESPACE_BEGIN

QuadTreeStats& GetQuadTreeStats()
{
    static QuadTreeStats stats;
    return stats;
}

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
	++GetQuadTreeStats().nodesCreated;

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

	// 有纹理时才创建网格；任务数用于判断加载是否结束。
	mPendingLayerTasks = mEarthNode->RequestTile(this);
}

QuadNode::~QuadNode()
{
	++GetQuadTreeStats().nodesDestroyed;

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

// 分裂与合并之间留迟滞区，避免阈值附近反复切换。
static constexpr double kSplitRatio = 1.0;
static constexpr double kMergeRatio = 1.45;

void QuadNode::Update(const EarthCameraPtr& camera)
{
	if (!camera)
	{
		return;
	}

	// 在渲染线程应用加载结果。
	ApplyLoadedTileData();

	// 无影像的节点无需 GPU 网格。
	if (mTexture != nullptr)
	{
		EnsureGpuBuffers();
	}

	if (IsGpuReady())
	{
		mStatusFlag |= FLAG_RENDER;
	}
	else
	{
		mStatusFlag &= ~FLAG_RENDER;
	}

	UpdateCullFlag(camera);

	const bool culled = HasFlag(mStatusFlag, FLAG_HAS_CULL);
	const double ratio = ComputeSplitRatio(camera);

	// 远处节点释放子树，包括已剔除的节点。
	if (HasChild() && ratio > kMergeRatio)
	{
		FreeChildNodes();
	}

	// 细化不等待本级影像，允许各级并行加载。
	if (!HasChild() && !culled && ratio < kSplitRatio)
	{
		CreateChildNodes();
	}

	// 迟滞区内也要更新子节点，及时应用加载结果。
	for (int i = 0; i < 4; ++i)
	{
		if (mChildNodes[i])
		{
			mChildNodes[i]->Update(camera);
		}
	}
}

void QuadNode::UpdateCullFlag(const EarthCameraPtr& camera)
{
	Matrix4x4d coloMatrix;
	Matrix4x4f viewProjMat = camera->GetProjectionMatrix() * camera->GetViewMatrix();
	for (uint16_t i = 0; i < 4; i++)
	{
		for (uint16_t j = 0; j < 4; j++)
		{
			coloMatrix[i][j] = viewProjMat[i][j];
		}
	}

	Frustumd frustum;
	frustum.InitFrustum(coloMatrix);

	if (frustum.IsBoxInFrustum(mBoundingBox))
	{
		mStatusFlag &= ~FLAG_HAS_CULL;
	}
	else
	{
		mStatusFlag |= FLAG_HAS_CULL;
	}
}

double QuadNode::ComputeSplitRatio(const EarthCameraPtr& camera) const
{
	const Vector3d vWSize = mBoundingBox.maximum - mBoundingBox.minimum;
	const double fSize = vWSize.Length() * 0.5;
	if (fSize <= 0.0)
	{
		return 1.0e30;
	}

	const Vector3f eyePosition = camera->GetPosition();
	const double distance = (mBoundingBox.center
		- Vector3d(eyePosition.x, eyePosition.y, eyePosition.z)).Length();

	return distance / fSize;
}

void QuadNode::CreateChildNodes()
{
	++GetQuadTreeStats().splits;

	const Vector2d vLlCenter = GetLonLatCenter();
	const Vector2d vLLHalf = GetLonLatRange() * 0.5;
	const uint32_t childLevel = mTileID.level + 1;

	mChildNodes[CHILD_LT] = std::make_shared<QuadNode>(mEarthNode, this
		, Vector2d(vLlCenter.x - vLLHalf.x, vLlCenter.y)
		, Vector2d(vLlCenter.x, vLlCenter.y + vLLHalf.y)
		, childLevel
		, CHILD_LT
	);

	mChildNodes[CHILD_RT] = std::make_shared<QuadNode>(mEarthNode, this
		, Vector2d(vLlCenter.x, vLlCenter.y)
		, Vector2d(vLlCenter.x + vLLHalf.x, vLlCenter.y + vLLHalf.y)
		, childLevel
		, CHILD_RT
	);

	mChildNodes[CHILD_LB] = std::make_shared<QuadNode>(mEarthNode, this
		, Vector2d(vLlCenter.x - vLLHalf.x, vLlCenter.y - vLLHalf.y)
		, Vector2d(vLlCenter.x, vLlCenter.y)
		, childLevel
		, CHILD_LB
	);

	mChildNodes[CHILD_RB] = std::make_shared<QuadNode>(mEarthNode, this
		, Vector2d(vLlCenter.x, vLlCenter.y - vLLHalf.y)
		, Vector2d(vLlCenter.x + vLLHalf.x, vLlCenter.y)
		, childLevel
		, CHILD_RB
	);
}

void QuadNode::FreeChildNodes()
{
	++GetQuadTreeStats().merges;

	for (int i = 0; i < 4; ++i)
	{
		mChildNodes[i] = nullptr;
	}
}

void QuadNode::GetRenderableNodes(QuadNodeArray& nodes)
{
	if (HasChild())
	{
		if (AreChildrenFullyReady())
		{
			for (int i = 0; i < 4; ++i)
			{
				mChildNodes[i]->GetRenderableNodes(nodes);
			}
			return;
		}

		// 四个子节点未全部就绪时绘制父节点，避免空洞和父子重叠。
	}

	if (IsDrawable())
	{
		nodes.push_back(this);
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

bool QuadNode::IsLoadSettled() const
{
	// 无数据的结果也算已完成。
	return mSettledLayerTasks >= mPendingLayerTasks;
}

bool QuadNode::IsDrawable() const
{
	return HasFlag(mStatusFlag, FLAG_RENDER) && HasNoFlag(mStatusFlag, FLAG_HAS_CULL) && IsGpuReady();
}

bool QuadNode::AreChildrenFullyReady() const
{
	for (int i = 0; i < 4; ++i)
	{
		const QuadNode* child = mChildNodes[i].get();
		if (child == nullptr || !child->IsGpuReady() || !child->IsLoadSettled())
		{
			return false;
		}
	}
	return true;
}

void QuadNode::EnsureGpuBuffers()
{
	RenderCore::RenderDevicePtr renderDevice = GetRenderDevice();
	if (!renderDevice)
	{
		return;
	}

	if (mIndexBuffer == nullptr || mVertexBuffer == nullptr)
	{
		// DEM 未到时先生成平地网格。
		mDemData.FillVertex();

		mIndexBuffer = renderDevice->CreateIndexBuffer(mDemData.GetFaceData(), mDemData.GetFaceBytes(), RenderCore::StorageModePrivate);
		mVertexBuffer = renderDevice->CreateVertexBuffer(mDemData.GetVertData(), mDemData.GetVertBytes(), RenderCore::StorageModePrivate);
		mInited = (mVertexBuffer != nullptr) && (mIndexBuffer != nullptr);
		mGeometryDirty = false;
	}
	else if (mGeometryDirty)
	{
		// DEM 到达后只重建顶点缓冲。
		mDemData.FillVertex();
		mVertexBuffer = renderDevice->CreateVertexBuffer(mDemData.GetVertData(), mDemData.GetVertBytes(), RenderCore::StorageModePrivate);
		mGeometryDirty = false;
	}
}

void QuadNode::ApplyLoadedTileData()
{
	RenderCore::RenderDevicePtr renderDevice = GetRenderDevice();
	if (!renderDevice)
	{
		return;
	}

	ObjectBasePtr loadedData;
	bool isTerrain = false;
	while (mLoadState->TakeLoadedData(loadedData, isTerrain))
	{
		// 空结果同样计入已完成任务。
		++mSettledLayerTasks;
		++GetQuadTreeStats().results;

		TiledImagePtr tiledImage = loadedData ? loadedData->toPtr<TiledImage>() : nullptr;
		if (!tiledImage)
		{
			++GetQuadTreeStats().emptyResults;
			continue;
		}

		if (isTerrain)
		{
			// DEM 先写 CPU 数据，网格由 EnsureGpuBuffers 更新。
			mDemData.FillHeight(tiledImage->heightData);
			mGeometryDirty = true;
			mStatusFlag |= FLAG_HAS_DEM;
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
			else
			{
				LOG_ERROR("QuadNode::ApplyLoadedTileData: 瓦片 %u/%u/%u 纹理创建失败",
					mTileID.level, mTileID.x, mTileID.y);
			}
		}
	}
}

EARTH_CORE_NAMESPACE_END
