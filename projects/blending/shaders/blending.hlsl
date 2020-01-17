const int MaxLights = 16;
const int NumLights = 1;

// Properties for all 3 kinds of lights - spot, point, directional.
struct LightProperties
{
  float3 strength;
  float  fallOffStart;
  float3 direction;
  float  fallOffEnd;
  float3 position;
  float  spotPower;
};

// Helper material struct used in lighting functions.
struct MaterialProperties
{
  float4 diffuseAlbedo;
  float3 fresnelR0;
  float  shininess;
};

Texture2D    DiffuseMap        : register(t0);
SamplerState LinearWrapSampler : register(s0);

// Per-pass constants.
cbuffer PassConstants : register(b0)
{
  float4x4 viewProjTransform;
  float3   eyePos;
  float    pad0;
  float4   ambientLight;
  Light    lights[MaxLights];
};

// Material constants.
cbuffer MaterialConstants : register(b1)
{
  float4x4 materialTransform;
  float4   diffuseAlbedo;
  float3   fresnelR0;
  float    roughness;
};

// Per-object properties.
cbuffer ObjConstants : register(b2)
{
    float4x4 worldTransform;
};

// Vertex shader.
void VS(float3 inPos : POSITION,
        float3 inNor : NORMAL,
        float2 inTex : TEXCOORD,
        out float4 outPosH : SV_POSITION,
        out float3 outPosW : POSITION,
        out float3 outNor  : NORMAL,
        out float2 outTexC : TEXCOORD)
{
  // Transform object to world space.
  float4 posW = mul(float4(inPos, 1.0f), worldTransform);
  outPosW = posw.xyz;

  outNor = mul(inNor, (float3x3)worldTransform);
  
  // Transform to homogenous clip space.
  outPosH = mul(posW, viewProjTransform);
  
  outTexC = mul(float4(inTex, 0.0f, 1.0f), materialTransform).xy;
}


// Lighting functions.
float3 BlinnPhong(float3 lightStrength, float3  lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    // Our spec formula goes outside [0,1] range, but we are 
    // doing LDR rendering.  So scale it down a bit.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.diffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 ComputeDirectionalLight(Light light, Material mat, float3 normal, float3 toEye)
{
    // The light vector aims opposite the direction the light rays travel.
    float3 lightVec = -light.Direction;

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = light.Strength * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}


float4 ComputeLighting(Light lights[MaxLights],
                       Material mat,
                       float3 pos,
                       float3 normal,
                       float3 toEye,
                       float3 shadowFactor)
{
  float3 result = 0.0f;
  int    i = 0;
  
  // Only directional lights for now.
  for (i = 0; i < NumLights; ++i) {
    result += shadowFactor[i] * ComputeDirectionalLights(lights[i], mat, normal, toEye);
  }

  return float4(result, 0.0f);
}

// Pixel shader.
float4 PS(float4 inPosH : SV_POSITION,
          float3 inPosW : POSITION,
          float3 inNor  : NORMAL,
          float2 inTexC : TEXCOORD) : SV_TARGET
{
  float4 fragColor = DiffuseMap.Sample(LinearWrapSampler, inTexC) * diffuseAlbedo;
  return fragColor;
  // Renormalize the normal.
  inNor = normalize(inNor);

  // Vector from point being lit to eye.
  float3 toEyeW = eyePos - inPosW;
  float distToEye = length(toEyeW);
  toEyeW /= distToEye;

  float3 shadowFactor = 1.0f;
  const float shininess = 1.0f - roughness;
  Material mat = { fragColor, fresnelR0, shininess };
  
  float4 litColor = ComputeLighting(lights, mat, inPosW, inNor,
                                    toEyeW, shadowFactor);

  litColor.a = diffuseAlbedo.a;

  return litColor;
}
