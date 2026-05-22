struct VSOut {
	float2 tex : TexCoord;
	float4 pos : SV_POSITION;
};

cbuffer CBuf {
	matrix transform;
};

VSOut main(float3 pos : POSITION, float2 tex: TexCoord)
{
	VSOut vsout;
	vsout.pos = mul(float4(pos.x, pos.y, pos.z, 1), transform);
	vsout.tex = tex;
	return vsout;
}