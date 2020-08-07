
struct SceneConstants{
    float4x4 gMvpMatrix;
};

ConstantBuffer<SceneConstants> Mvp : register(b0, space0);

struct VertexIn {
    float3 PosL  : POSITION;
    float4 Color : COLOR;
};

struct VertexOut {
    float4 PosH  : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut SimpleVS(VertexIn vIn) {
    VertexOut vOut;
    vOut.PosH  = mul(float4(vIn.PosL, 1.0f), Mvp.gMvpMatrix);
    vOut.Color = vIn.Color;
    return vOut;
}

float4 SimplePS(VertexOut vOut) : SV_Target {
    return vOut.Color;
}