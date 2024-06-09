//
//  TileDraw.metal
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

struct VertexIn
{
    float2 position    : POSITION;   //顶点坐标
    float2 tex : TEXCOORD0;    //纹理坐标
};

struct VertexOut
{
    float4 PosH : SV_POSITION;  //顶点输出坐标，裁剪空间
    float2 tex : TEXCOORD0;
};

cbuffer cbPerObject : register(b0, space0)
{
    float4x4 MATRIX_MVP;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vertexOut;
    vertexOut.PosH = mul(float4(vin.position, 0.0, 1.0), MATRIX_MVP);
    vertexOut.tex = vin.tex;
    
    return vertexOut;
}

Texture2D gColorMap : register(t0, space1);
SamplerState gColorMapSamp : register(s0, space2);

float4 PS(VertexOut pin) : SV_Target
{
    return gColorMap.Sample(gColorMapSamp, pin.tex);
}
