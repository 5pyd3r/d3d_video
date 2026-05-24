Texture2D<float4> tex : t0;
SamplerState splr;

float4 main(float2 tc: TexCoord) : SV_Target
{
    return tex.Sample(splr, tc);
}
