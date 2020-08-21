
struct SceneConstants{
    float4x4 gMvpMatrix;
};

ConstantBuffer<SceneConstants> Mvp : register(b0, space0);

TextureCube gCubeMap : register(t0);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

struct VertexIn {
    float3 PosL  : POSITION;
    float4 Color : COLOR;
};

struct VertexOut {
    float4 PosH  : SV_POSITION;
    float3 posL  : POSITION;
    float4 Color : COLOR;
};

VertexOut SimpleVS(VertexIn vIn) {
    VertexOut vOut;
    vOut.posL = vIn.PosL;
    vOut.PosH  = mul(float4(vIn.PosL, 1.0f), Mvp.gMvpMatrix).xyww;
    vOut.Color = vIn.Color;
    return vOut;
}

float4 SimplePS(VertexOut vOut) : SV_Target {
    //return vOut.Color;
    return gCubeMap.Sample(gsamLinearWrap, vOut.posL);
}