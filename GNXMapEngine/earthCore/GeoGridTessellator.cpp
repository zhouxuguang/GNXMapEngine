//
//  GeoGridTessellator.cpp
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright © 2018年 Zhou,Xuguang. All rights reserved.
//

#include "GeoGridTessellator.h"
#include "SubdivisionUtility.h"

EARTH_CORE_NAMESPACE_BEGIN

static int NumberOfTriangles(int numberOfSlicePartitions, int numberOfStackPartitions)
{
    int numberOfTriangles = 2 * numberOfSlicePartitions;                                // Top and bottom fans
    numberOfTriangles += 2 * ((numberOfStackPartitions - 2) * numberOfSlicePartitions); // Middle triangle strips
    return numberOfTriangles;
}

static int NumberOfVertices(int numberOfSlicePartitions, int numberOfStackPartitions)
{
    return 2 + ((numberOfStackPartitions - 1) * numberOfSlicePartitions);
}

MeshPtr GeoGridTessellator::Compute(const Ellipsoid& ellipsoid,
                                 int numberOfSlicePartitions,
                                 int numberOfStackPartitions,
                                 GeoGridVertexAttributes vertexAttributes
                                 )
{
    std::vector<Vector3d> vecPosition;
    std::vector<Vector3f> vecNormal;
    std::vector<Vector2f> vecTexturePoint;
    std::vector<uint32_t> vecVertexIndice;
    
    //经度划分网格数
    if (numberOfSlicePartitions < 3)
    {
        return;
    }
    
    //纬度划分数
    if (numberOfStackPartitions < 2)
    {
        return;
    }
    
    //
    // 创建查找表
    //
//    std::vector<double> cosTheta(numberOfSlicePartitions);
//    std::vector<double> sinTheta(numberOfSlicePartitions);
//    
//    for (int j = 0; j < numberOfSlicePartitions; ++j)
//    {
//        double theta = (M_PI * 2.0) * (((double)j) / numberOfSlicePartitions);
//        cosTheta[j] = cos(theta);
//        sinTheta[j] = sin(theta);
//    }
    
    //
    // 创建顶点
    //
    
    int numberOfVertices = NumberOfVertices(numberOfSlicePartitions, numberOfStackPartitions);
    vecPosition.reserve(numberOfVertices);
    
    Vector3d axis = ellipsoid.GetAxis();
    
    // 北极点
    vecPosition.emplace_back(0, 0, axis.z);
    
    for (int i = 1; i < numberOfStackPartitions; ++i)
    {
        double phi = M_PI * (((double)i) / numberOfStackPartitions);
        double sinPhi = sin(phi);
        double cosPhi = cos(phi);
        
        for (int j = 0; j < numberOfSlicePartitions; ++j)
        {
            double theta = (M_PI * 2.0) * (((double)j) / numberOfSlicePartitions);
            double cosTheta = cos(theta);
            double sinTheta = sin(theta);
            
            vecPosition.emplace_back(axis.x * cosTheta * sinPhi, axis.y * sinTheta * sinPhi, axis.z * cosPhi);
        }
    }
    vecPosition.emplace_back(0, 0, -axis.z);  //南极点
    
    //计算法向量和纹理坐标
    if ((vertexAttributes & GeoGridVertexAttributes::Normal) == GeoGridVertexAttributes::Normal ||
        (vertexAttributes & GeoGridVertexAttributes::TextureCoordinate) == GeoGridVertexAttributes::TextureCoordinate)
    {
        vecNormal.reserve(numberOfVertices);
        vecTexturePoint.reserve(numberOfVertices);
        for (int i = 0; i < vecPosition.size(); ++i)
        {
            Vector3d deticSurfaceNormal = ellipsoid.GeodeticSurfaceNormal(vecPosition[i]);
            vecNormal.emplace_back(deticSurfaceNormal.x, deticSurfaceNormal.y, deticSurfaceNormal.z);
            
            vecTexturePoint.push_back(SubdivisionUtility::ComputeTextureCoordinate(deticSurfaceNormal));
        }
    }
    
    //计算索引数组
    int nIndiceSize = 3 * NumberOfTriangles(numberOfSlicePartitions, numberOfStackPartitions);
    vecVertexIndice.reserve(nIndiceSize);
    
    //
    // Triangle fan top row
    for (int j = 1; j < numberOfSlicePartitions; ++j)
    {
        vecVertexIndice.push_back(0);
        vecVertexIndice.push_back(j);
        vecVertexIndice.push_back(j+1);
    }
    vecVertexIndice.push_back(0);
    vecVertexIndice.push_back(numberOfSlicePartitions);
    vecVertexIndice.push_back(1);
    
    //
    // Middle rows are triangle strips
    //
    for (int i = 0; i < numberOfStackPartitions - 2; ++i)
    {
        int topRowOffset = (i * numberOfSlicePartitions) + 1;
        int bottomRowOffset = ((i + 1) * numberOfSlicePartitions) + 1;
        
        for (int j = 0; j < numberOfSlicePartitions - 1; ++j)
        {
            //左下三角形
            vecVertexIndice.push_back(bottomRowOffset + j);
            vecVertexIndice.push_back(bottomRowOffset + j + 1);
            vecVertexIndice.push_back(topRowOffset + j + 1);
            
            //右上三角形
            vecVertexIndice.push_back(bottomRowOffset + j);
            vecVertexIndice.push_back(topRowOffset + j + 1);
            vecVertexIndice.push_back(topRowOffset + j);
        }
        vecVertexIndice.push_back(bottomRowOffset + numberOfSlicePartitions - 1);
        vecVertexIndice.push_back(bottomRowOffset);
        vecVertexIndice.push_back(topRowOffset);
        
        vecVertexIndice.push_back(bottomRowOffset + numberOfSlicePartitions - 1);
        vecVertexIndice.push_back(topRowOffset);
        vecVertexIndice.push_back(topRowOffset + numberOfSlicePartitions - 1);
    }
    
    //
    // Triangle fan bottom row
    //
    unsigned int lastPosition = (unsigned int)vecPosition.size() - 1;
    for (unsigned int j = lastPosition - 1; j > lastPosition - numberOfSlicePartitions; --j)
    {
        vecVertexIndice.push_back(lastPosition);
        vecVertexIndice.push_back(j);
        vecVertexIndice.push_back(j-1);
    }
    vecVertexIndice.push_back(lastPosition);
    vecVertexIndice.push_back(lastPosition - numberOfSlicePartitions);
    vecVertexIndice.push_back(lastPosition - 1);
    
    MeshPtr mesh = std::make_shared<Mesh>();
    
    // 设置顶点布局
    VertexData & vertexData = mesh->GetVertexData();
    ChannelInfo* channels = vertexData.GetChannels();
    uint32_t offset = 0;
    uint32_t vertexSize = 0;
    if (!vecPosition.empty())
    {
        channels[kShaderChannelPosition].offset = offset;
        channels[kShaderChannelPosition].format = VertexFormatFloat4;
        vertexSize += 16;
        offset += vecPosition.size() * sizeof(Vector4f);
    }
    if (!vecNormal.empty())
    {
        channels[kShaderChannelNormal].offset = offset;
        channels[kShaderChannelNormal].format = VertexFormatFloat4;
        vertexSize += 16;
        offset += vecNormal.size() * sizeof(Vector4f);
    }
    if (!vecTexturePoint.empty())
    {
        channels[kShaderChannelTexCoord0].offset = offset;
        channels[kShaderChannelTexCoord0].format = VertexFormatFloat2;
        vertexSize += 8;
        offset += vecTexturePoint.size() * sizeof(Vector2f);
    }
    
    channels[kShaderChannelPosition].stride = sizeof(Vector4f);
    channels[kShaderChannelNormal].stride = sizeof(Vector4f);
    channels[kShaderChannelColor].stride = 0;
    channels[kShaderChannelTexCoord0].stride = sizeof(Vector2f);
    channels[kShaderChannelTexCoord1].stride = 0;
    channels[kShaderChannelTangent].stride = 0;
    
    mesh->GetVertexData().Resize((uint32_t)vecPosition.size(), vertexSize);
    
    std::vector<Vector4f> tempPos;
    tempPos.reserve(vecPosition.size());
    for (auto & iter : vecPosition)
    {
        tempPos.emplace_back(iter.x, iter.y, iter.z, 1.0);
    }
    mesh->SetPositions(tempPos.data(), tempPos.size());
    
    std::vector<Vector4f> tempNormal;
    tempNormal.reserve(vecNormal.size());
    for (auto & iter : vecNormal)
    {
        tempNormal.emplace_back(iter.x, iter.y, iter.z, 1.0);
    }
    mesh->SetNormals(tempNormal.data(), tempNormal.size());
    mesh->SetUv(0, vecTexturePoint.data(), vecTexturePoint.size());
    mesh->SetIndices(vecVertexIndice.data(), vecVertexIndice.size());
    
    SubMeshInfo subInfo;
    subInfo.firstIndex = 0;
    subInfo.indexCount = (uint32_t)vecVertexIndice.size();
    subInfo.vertexCount = (uint32_t)tempPos.size();
    subInfo.topology = PrimitiveMode_TRIANGLES;
    mesh->AddSubMeshInfo(subInfo);
    
    //构建gpu资源
    mesh->SetUpBuffer();
    
    return mesh;
}

EARTH_CORE_NAMESPACE_END
