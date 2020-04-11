cbuffer ShaderMvpCb : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VsIn
{
    float3 position : POSITION;
};

struct VsOut
{
    float4 posWorld : SV_POSITION;
};

VsOut VS(VsIn vin)
{
    VsOut vout;

    // Transform to homogeneous clip space.
    vout.posWorld = mul(float4(vin.position, 1.0f), gWorldViewProj);

    return vout;
};


float4 PS(VsOut fromVs) : SV_TARGET
{
    return float4 (1.0f, 0.8f, 0.8f, 1.0f);
};
