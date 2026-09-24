#include "EarthRenderer.h"
#include "EarthNode.h"
#include "Runtime/BaseLib/include/LogService.h"

EARTH_CORE_NAMESPACE_BEGIN

EarthRenderer::EarthRenderer()
{
	SamplerDesc samplerDescriptor;
	mSampler = GetRenderDevice()->CreateSamplerWithDescriptor(samplerDescriptor);
}

EarthRenderer::~EarthRenderer()
{
}

void EarthRenderer::SetRendererNodes(const QuadNode::QuadNodeArray& nodes)
{
	//mNodes = nodes;
}

void EarthRenderer::Render(RenderInfo& renderInfo)
{
	EarthNode* earthNode = (EarthNode*)mSceneNode;

	QuadNode::QuadNodeArray quadNodes;
	earthNode->GetAllRendererNodes(quadNodes);

	RenderEncoderPtr renderEncoder = renderInfo.renderEncoder;
	assert(renderEncoder);

	renderInfo.materials = GetMaterials();

	renderEncoder->SetGraphicsPipeline(renderInfo.materials[0]->GetPSO());

	/*const ChannelInfo* channels = mesh.GetVertexData().GetChannels();
	VertexBufferPtr vertexBuffer = mesh.GetVertexBuffer();
	IndexBufferPtr indexBuffer = mesh.GetIndexBuffer();*/

	for (int n = 0; n < quadNodes.size(); n++)
	{
		// 双重保险：GPU 资源没备齐的瓦片绝不能提交绘制。
		// 纹理为空时 gDiffuseMap 描述符从未被 push，shader 会采样到未初始化的
		// push descriptor（内容未定义，可能是垃圾 imageView），驱动读取非法描述符
		// 会直接导致 VK_ERROR_DEVICE_LOST。
		if (!quadNodes[n]->IsGpuReady() || !quadNodes[n]->GetTexture())
		{
			continue;
		}

		renderEncoder->SetVertexUniformBuffer("cbPerCamera", renderInfo.cameraUBO);
		renderEncoder->SetVertexUniformBuffer("cbPerObject", quadNodes[n]->mLocalUniform);
		renderEncoder->SetVertexUniformBuffer("LightInfo", renderInfo.lightUBO);

		renderEncoder->SetFragmentUniformBuffer("cbPerCamera", renderInfo.cameraUBO);
		renderEncoder->SetFragmentUniformBuffer("LightInfo", renderInfo.lightUBO);

		renderEncoder->SetVertexBuffer(quadNodes[n]->mVertexBuffer, 0, 0);
		uint32_t offset = quadNodes[n]->mDemData.GetVertCount() * sizeof(Vector3f);
		renderEncoder->SetVertexBuffer(quadNodes[n]->mVertexBuffer, offset, 1);
		offset += quadNodes[n]->mDemData.GetVertCount() * sizeof(Vector3f);
		renderEncoder->SetVertexBuffer(quadNodes[n]->mVertexBuffer, offset, 3);
		/*renderEncoder->setVertexBuffer(vertexBuffer, channels[kShaderChannelTangent].offset, 2);
		renderEncoder->setVertexBuffer(vertexBuffer, channels[kShaderChannelTexCoord0].offset, 3);*/

		MaterialPtr material = renderInfo.materials[0];
		assert(material);

		//TextureSamplerPtr textureSampler = mesh.GetSampler();

		//这里感觉采样器和纹理封装在一个对象里面会比较方便
		renderEncoder->SetFragmentTextureAndSampler("gDiffuseMap", quadNodes[n]->mTexture, mSampler);
		/*renderEncoder->setFragmentTextureAndSampler("gNormalMap", material->GetTexture("normalTexture"), textureSampler);
		renderEncoder->setFragmentTextureAndSampler("gMetalRoughMap", material->GetTexture("roughnessTexture"), textureSampler);
		renderEncoder->setFragmentTextureAndSampler("gEmissiveMap", material->GetTexture("emissiveTexture"), textureSampler);
		renderEncoder->setFragmentTextureAndSampler("gAmbientMap", material->GetTexture("ambientTexture"), textureSampler);*/

		//const SubMeshInfo& subInfo = mesh.GetSubMeshInfo(n);

		renderEncoder->DrawIndexedPrimitives(PrimitiveMode_TRIANGLES, 
			(int)quadNodes[n]->mDemData.GetFaceCount() * 3, 
			quadNodes[n]->mIndexBuffer, 0, 0, RenderCore::IndexType_UShort);
		static bool loggedFirstTexturedTile = false;
		if (!loggedFirstTexturedTile)
		{
			LOG_INFO("[Earth] First uploaded tile submitted for textured draw");
			loggedFirstTexturedTile = true;
		}

	}
}

EARTH_CORE_NAMESPACE_END
