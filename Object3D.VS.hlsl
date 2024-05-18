#include "Object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 pos : POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

VertexShaderOutput main( VertexShaderInput input )
{
    VertexShaderOutput output;
    output.pos = mul( input.pos, gTransformationMatrix.WVP );
    output.texcoord = input.texcoord;
    return output;
}