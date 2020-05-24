
struct SceneConstants{
    float4x4 gMvpMatrix;
};

struct PerObjectData {
    uint materialIndex;
};

struct MaterialData {
    float4 color;
};

ConstantBuffer<SceneConstants> Mvp : register(b0, space0);
ConstantBuffer<PerObjectData> ObjData : register(b1, space0);
StructuredBuffer<MaterialData> Materials : register(t0, space0);

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
    //vOut.Color = vIn.Color;
    vOut.Color = Materials[ObjData.materialIndex].color;
    return vOut;
}

float4 SimplePS(VertexOut vOut) : SV_Target {
    return vOut.Color;
}