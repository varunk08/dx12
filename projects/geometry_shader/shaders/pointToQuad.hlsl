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
    float3 posWorld : POSITION;
};


struct GeoOut
{
	float4 posHom    : SV_POSITION;
    uint   primId  : SV_PrimitiveID;
};

// Simple pass through vertex shader.
VsOut VS(VsIn vin)
{
    VsOut vout;

    vout.posWorld = vin.position;

    return vout;
};

[maxvertexcount(4)]
void GS(point VsOut vsOut[1],
          uint primId : SV_PRIMITIVEID,
          inout TriangleStream<GeoOut> triStream)
{
    float halfWidth  = 0.1f;
    float halfHeight = 0.1f;

    float3 right = float3(1.0f, 0.0f, 0.0f);
    float3 up    = float3(0.0f, 1.0f, 0.0f);

    // Create new verts offset from the input vertex.
    float4 newVerts[4];
    newVerts[0] = float4(vsOut[0].posWorld + halfWidth * right - halfHeight * up, 1.0f);
    newVerts[1] = float4(vsOut[0].posWorld + halfWidth * right + halfHeight * up, 1.0f);
    newVerts[2] = float4(vsOut[0].posWorld - halfWidth * right - halfHeight * up, 1.0f);
    newVerts[3] = float4(vsOut[0].posWorld - halfWidth * right + halfHeight * up, 1.0f);

    GeoOut gsOut;
    [unroll]
    for (int i = 0; i < 4; i++) {
        // Transform to homogeneous clip space.
        gsOut.posHom = mul(newVerts[i], gWorldViewProj);
        gsOut.primId = primId;
        triStream.Append(gsOut);
    }
};

// Simple pixel shader.
float4 PS(GeoOut fromVs) : SV_TARGET
{
    return float4 (1.0f, 0.8f, 0.8f, 1.0f);
};
