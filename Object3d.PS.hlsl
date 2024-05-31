#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
};

struct PixelShaderOutput
{
    float32_t4 color;
};

struct BloomThreshold
{
    float32_t threshold;
    
    float32_t2 blurSize;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<BloomThreshold> gBloomThreshold : register(b1);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 BloomExtract(float2 texcoord)
{
    float4 color = gTexture.Sample(gSampler, texcoord);
    return saturate((color - gBloomThreshold.threshold) / (1.0f - gBloomThreshold.threshold));
}

float4 AdjustSaturation(float4 color, float saturation)
{
    float gray = dot(color.rgb, float3(0.3f, 0.59f, 0.11f));
    return lerp(gray, color, saturation);
}

float4 GaussianBlur(float2 texcoord, float2 texSize)
{
    // 1ピクセルの長さ
    const float2 texOffset = 1.0f / texSize;
    
    // Gaussian Kernel
    const float weight[5] = { 0.227027f, 0.19459465f, 0.1216216f, 0.054054f, 0.016216f };
    
    // 画像の明るい部分を抽出
    float4 bColor = BloomExtract(texcoord);
    
    float3 result;
    result.x = bColor.x * weight[0];
    result.y = bColor.y * weight[0];
    result.z = bColor.z * weight[0];
    
    // 画像の明るい部分を横方向にぼかす
    for (int i = 1; i < 5; i++)
    {
        float2 weightOffset = float2(texOffset.x * i, 0.0f);
        result += BloomExtract(texcoord + weightOffset * gBloomThreshold.blurSize).xyz * weight[i];
        result += BloomExtract(texcoord - weightOffset * gBloomThreshold.blurSize).xyz * weight[i];
    }
   
    // 画像の明るい部分を縦方向にぼかす
    for (int j = 1; j < 5; j++)
    {
        float2 weightOffset = float2(0.0f, texOffset.y * j);
        result += BloomExtract(texcoord + weightOffset * gBloomThreshold.blurSize).xyz * weight[j];
        result += BloomExtract(texcoord - weightOffset * gBloomThreshold.blurSize).xyz * weight[j];
    }
    
    return float4(result, 1.0f);
}

float4 BloomCombine(float2 texcoord, float2 texSize)
{
    //float4 baseColor = gTexture.Sample(gSampler, texcoord);
    //float4 blurColor = GaussianBlur(texcoord, texSize);
    
    float4 baseColor = AdjustSaturation(gTexture.Sample(gSampler, texcoord), 1.0f);
    float4 blurColor = AdjustSaturation(GaussianBlur(texcoord, texSize), 0.6f);
    
    return baseColor + blurColor;
}


PixelShaderOutput main(VertexShaderOutput input) : SV_TARGET0
{
    PixelShaderOutput output;
    
    float32_t2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);
    
    output.color = BloomCombine(input.texcoord, texSize);
    
    //output.color = BloomExtract(input.texcoord);
   
    
    return output;
}