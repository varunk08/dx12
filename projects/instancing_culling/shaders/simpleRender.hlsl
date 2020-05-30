
struct SceneConstants{
    float4x4 gMvpMatrix;
};

struct PerObjectData {
    uint materialIndex;
    uint padding0;
    uint padding1;
    uint padding2;
};

struct MaterialData {
    float4 color;
};

struct VertexIn {
    float3 PosL  : POSITION;
    float4 Color : COLOR;
    float2 texUV : TEXCOORD;
};

struct VertexOut {
    float4 PosH  : SV_POSITION;
    float4 Color : COLOR;
    float2 texUV : TEXCOORD;
};

ConstantBuffer<SceneConstants> Mvp         : register(b0, space0);
ConstantBuffer<PerObjectData>  ObjData     : register(b1, space0);
StructuredBuffer<MaterialData> Materials   : register(t0, space1);
Texture2D                      Textures[3] : register(t0, space0);

SamplerState gSamPointWrap        : register(s0);
SamplerState gSamPointClamp       : register(s1);
SamplerState gSamLinearWrap       : register(s2);
SamplerState gSamLinearClamp      : register(s3);
SamplerState gSamAnisotropicWrap  : register(s4);
SamplerState gSamAnisotropicClamp : register(s5);

VertexOut SimpleVS(VertexIn vIn) {
    VertexOut vOut;
    vOut.PosH  = mul(float4(vIn.PosL, 1.0f), Mvp.gMvpMatrix);
    vOut.Color = Materials[ObjData.materialIndex].color;
    vOut.texUV = vIn.texUV;
    return vOut;
}

float4 SimplePS(VertexOut vOut) : SV_Target {
    float4 outColor;
    outColor = float4(vOut.texUV, 0.0f, 1.0f);
    outColor = Textures[ObjData.materialIndex].Sample(gSamLinearWrap, vOut.texUV);

    return outColor;
}