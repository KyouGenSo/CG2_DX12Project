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
    output.pos = input.pos;
    return output;
}