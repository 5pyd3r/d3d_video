#ifdef NV12_INPUT

Texture2D<float>  luminanceChannel   : register(t0);
Texture2D<float2> chrominanceChannel : register(t1);

SamplerState splr;

static const float3x3 YUVtoRGBCoeffMatrix =
{
    1.164383f,  1.164383f, 1.164383f,
    0.000000f, -0.391762f, 2.017232f,
    1.596027f, -0.812968f, 0.000000f
};

float3 ConvertYUVtoRGB(float3 yuv)
{
    yuv -= float3(0.062745f, 0.501960f, 0.501960f);
    yuv = mul(yuv, YUVtoRGBCoeffMatrix);
    return saturate(yuv);
}

float4 main(float2 tc : TEXCOORD) : SV_Target
{
    float y = luminanceChannel.Sample(splr, tc);
    float2 uv = chrominanceChannel.Sample(splr, tc);
    float3 rgb = ConvertYUVtoRGB(float3(y, uv));
    float gray = dot(rgb, float3(0.299f, 0.587f, 0.114f));
    return float4(gray, gray, gray, 1.0f);
}

#else // BGRA_INPUT

Texture2D<float4> tex : register(t0);
SamplerState splr;

float4 main(float2 tc : TEXCOORD) : SV_Target
{
    float4 color = tex.Sample(splr, tc);
    float gray = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    return float4(gray, gray, gray, 1.0f);
}

#endif
