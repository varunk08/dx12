cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VertexIn
{
    float3 position : POSITION;
};

struct VertexOut
{
    float4 posHomogenous : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Transform to homogeneous clip space.
    vout.posHomogenous = mul(float4(vin.position, 1.0f), gWorldViewProj);

    return vout;
};


float4 PS(VertexOut fromVs) : SV_TARGET
{
    return float4 (1.0f, 0.8f, 0.8f, 1.0f);
};
