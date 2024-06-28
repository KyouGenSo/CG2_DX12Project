#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    if (gMaterial.enableLighting != 0)
    {
        float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
        output.color = gTexture.Sample(gSampler, input.texcoord) * gMaterial.color * gDirectionalLight.color * gDirectionalLight.intensity * cos;
    }
    else
    {
        output.color = gTexture.Sample(gSampler, input.texcoord) * gMaterial.color;
    }
    
    return output;
}