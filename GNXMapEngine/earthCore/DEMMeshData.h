//
//  DEMMeshData.h
//  earthEngineCore
//
//  Created by Zhou,Xuguang on 2018/12/23.
//  Copyright  2018 年 Zhou,Xuguang. All rights reserved.
//

#ifndef GNX_MAPENGINE_DEMMESH_DATA_INCLUDE_KSFGJSDNGKDFNGN
#define GNX_MAPENGINE_DEMMESH_DATA_INCLUDE_KSFGJSDNGKDFNGN

#include "Ellipsoid.h"

EARTH_CORE_NAMESPACE_BEGIN

const uint16_t DEM_WIDTH = 65;
const uint16_t DEM_HEIGHT = 65;

struct Vertex
{
	mathutil::simd_float4 position;
	mathutil::simd_float4 normal;
	mathutil::simd_float2 texCoord;
};

template<uint16_t row, uint16_t col>
class DemMeshData
{
private:
    struct DemVertex
    {
		mathutil::simd_float4 position[row * col];
		mathutil::simd_float4 normal[row * col];
		mathutil::simd_float2 texCoord[row * col];
    };

    DemVertex mVertexData;

    uint16_t mFaces[(row -1) * (col - 1) * 6];
    uint16_t mRow = 0;
    uint16_t mCol = 0;
public:
    DemMeshData()
    {
        mRow = row;
        mCol = col;
    }

    int GetRows() const
    {
        return mRow;
    }

    int GetCols() const
    {
        return mCol;
    }

    DemVertex* GetVertData()
    {
        return (DemVertex*)mVertexData.position;
    }

    const DemVertex* GetVertData() const
    {
        return (DemVertex*)mVertexData.position;
    }

    int GetVertCount() const
    {
        return mRow * mCol;
    }

    int GetVertBytes() const
    {
        return GetVertCount() * sizeof(Vertex);
    }

    uint16_t* GetFaceData()
    {
        return mFaces;
    }

    const uint16_t* GetFaceData() const
    {
        return mFaces;
    }

    int GetFaceCount() const
    {
        return (mRow -1) * (mCol - 1) * 2;
    }

    int GetFaceBytes() const
    {
        return GetFaceCount() * sizeof(uint16_t) * 3;
    }

    void FillFace()
    {
        uint32_t index = 0;
        for (uint16_t r = 0; r < mRow - 1; ++r)
        {
            for (uint16_t c = 0; c < mCol - 1; ++c)
            {
                mFaces[index + 0] = (mCol * r) + c;
                mFaces[index + 1] = (mCol * r) + c + 1;
                mFaces[index + 2] = (mCol * (r + 1)) + c;

                mFaces[index + 3] = (mCol * r) + c + 1;
                mFaces[index + 4] = mCol * (r + 1) + c + 1;
                mFaces[index + 5] = mCol * (r + 1) + c;
                index += 6;
            }
        }
    }

    void FillVertex(const mathutil::Vector2d& vStart, const mathutil::Vector2d& vEnd, const Ellipsoid& ellipsoid)
    {
        mathutil::Vector2d vSize = vEnd - vStart;
        mathutil::Vector2d vGrid = mathutil::Vector2d(vSize.x/(mCol -1), vSize.y/(mRow - 1));

		for (uint16_t r = 0; r < mRow; ++r)
		{
			for (uint16_t c = 0; c < mCol; ++c)
			{
				int idx = r * mCol + c;
                mathutil::Vector3d vWorld = ellipsoid.CartographicToCartesian(Geodetic3D(vStart.x + c * vGrid.x, vStart.y + r * vGrid.y));
                mathutil::Vector3d normal = ellipsoid.GeodeticSurfaceNormal(Geodetic3D(vStart.x + c * vGrid.x, vStart.y + r * vGrid.y));
				
                mVertexData.position[idx].x = vWorld.x;
                mVertexData.position[idx].y = vWorld.y;
                mVertexData.position[idx].z = vWorld.z;
                mVertexData.position[idx].w = 1.0;

				mVertexData.normal[idx].x = normal.x;
				mVertexData.normal[idx].y = normal.y;
				mVertexData.normal[idx].z = normal.z;
				mVertexData.normal[idx].w = 1.0;
			}
		}
    }

    void FillUV(const mathutil::Vector2f& vStart, const mathutil::Vector2f& vEnd)
    {
        mathutil::Vector2f uvSize = vEnd - vStart;
        mathutil::Vector2f uvStep = mathutil::Vector2f(uvSize.x/(mCol -1), uvSize.y/(mRow - 1));
        for (uint16_t r = 0; r < mRow; ++r)
        {
            for (uint16_t c = 0; c < mCol; ++c)
            {
                int idx = r * mCol + c;
                mVertexData.texCoord[idx].x = (vStart.x + double(c) * uvStep.x);
                mVertexData.texCoord[idx].y = (vStart.y + double(r) * uvStep.y);
            }
        }
    }

	void FillHeight()
	{
        float* pData = nullptr;
		for (size_t i = 0;i < mRow * mCol; ++i)
		{
			mVertexData[i].h = pData[i];
		}
	}
};

using DemData = DemMeshData<DEM_WIDTH, DEM_HEIGHT>;

EARTH_CORE_NAMESPACE_END

#endif