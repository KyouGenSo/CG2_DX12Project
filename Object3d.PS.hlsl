#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
};

struct TexVisibility
{
    float32_t visible;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<TexVisibility> gTexVisibility : register(b1);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);


PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    if(gTexVisibility.visible == 1.0f)
    output.color = gTexture.Sample(gSampler, input.texcoord) * gMaterial.color;
    else
    output.color = gMaterial.color;
    
    return output;
}