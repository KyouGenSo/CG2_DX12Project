struct TransformationMatrix
{
    float32_t4x4 WVP;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
};

struct VertexShaderInput
{
    float4 pos : POSITION;
};

VertexShaderOutput main( VertexShaderInput input )
{
    VertexShaderOutput output;
    output.pos = mul( input.pos, gTransformationMatrix.WVP );
    return output;
}