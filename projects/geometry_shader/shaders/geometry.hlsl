Texture2D    gDiffuseMap : register(t0);


SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer ObjectConstants : register (b0)
{
    float4x4 worldTransform;
};

cbuffer PassConstants : register (b1)
{
    float4x4 viewProjTransform;
};

struct VertexIn
{
    float3 posL : POSITION;
    float3 normalL : NORMAL;
    float2 texC : TEXCOORD;
};

struct VertexOut
{
    float4 posH : SV_POSITION;
    float3 posW : POSITION;
    float3 normalW : NORMAL;
    float2 texC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    float4 posW = mul(float4(vin.posL, 1.0f), worldTransform);
    vout.posW = posW.xyz;
    vout.normalW = mul(vin.normalL, (float3x3) worldTransform);
    vout.posH = mul(posW, viewProjTransform);
    vout.texC = vin.texC;

    return vout;
};


float4 PS(VertexOut pin) : SV_TARGET
{
    return float4(gDiffuseMap.Sample(gsamAnisotropicWrap, pin.texC));
};
