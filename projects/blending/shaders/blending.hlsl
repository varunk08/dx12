const int MaxLights = 16;
const int NumLights = 1;

// Properties for all 3 kinds of lights - spot, point, directional.
struct Light
{
  float3 strength;
  float fallOffStart;
  float3 direction;
  float fallOffEnd;
  float3 position;
  float spotPower;
};

struct Material
{
  float4 diffuseAlbedo;
  float3 fresnelR0;
  float shininess;
};

Texture2D diffuse_map : register(t0);
SamplerState linear_wrap_samp : register(s0);

// Vertex shader.
cbuffer pass_constants : register(b0)
{
  float4x4 view_proj;
  float3 eye_pos;
  float pad0;
  Light lights[MaxLights];
};

// Material constants.
cbuffer material_cbuf : register(b1)
{
  float4x4 mat_transform;
  float4   diffuse_albedo;
  float3   fresnelR0;
  float    roughness;
};

cbuffer obj_constants : register(b2)
{
    float4x4 world_view_proj;
};

// Vertex shader.
void VS(float3 in_pos : POSITION,
        float3 in_nor : NORMAL,
        float2 in_tex : TEXCOORD,
        out float4 out_pos_h : SV_POSITION,
        out float3 out_pos_w : POSITION,
        out float3 out_nor   : NORMAL,
        out float2 out_texC  : TEXCOORD)
{
  // Transform object to world space.
  float4 pos_w = mul(float4(in_pos, 1.0f), world_view_proj);
  out_pos_w = pos_w.xyz;

  out_nor = mul(in_nor, (float3x3)world_view_proj);
  
  // Transform to homogenous clip space.
  out_pos_h = mul(pos_w, view_proj);
  
  out_texC = mul(float4(in_tex, 0.0f, 1.0f), mat_transform).xy;
}


// Lighting functions.
float3 BlinnPhong(float3 lightStrength, float3  lightVec, float3 normal, float3 toEye, Material mat)
{
}

float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
  
}


float4 ComputeLighting(Light gLights[MaxLights], Material mat,
                       float3 pos, float3 normal, float3 toEye,
                       float3 shadowFactor)
{
  float3 result = 0.0f;
  int i = 0;
  
  // Only directional lights for now.
  for (i = 0; i < NumLights; ++i) {
    result += shadowFactor[i] * ComputeDirectionalLights(lights[i], mat, normal, toEye);
  }

  return float4(result, 0.0f);
}

// Pixel shader.
float4 PS(float4 in_pos_h : SV_POSITION,
          float3 in_pos_w : POSITION,
          float3 in_nor   : NORMAL,
          float2 in_texC  : TEXCOORD) : SV_TARGET
{
  float4 fragColor = diffuse_map.Sample(linear_wrap_samp, in_texC) * diffuse_albedo;

  // Renormalize the normal.
  in_nor = normalize(in_nor);

  // Vector from point being lit to eye.
  float3 toEyeW = eye_pos - in_pos_w;
  float distToEye = length(toEyeW);
  toEyeW /= distToEye;

  float3 shadowFactor = 1.0f;
  const float shininess = 1.0f - roughness;
  Material mat = { fragColor, fresnelR0, shininess };
  float4 litColor = ComputeLighting(lights, mat, in_pos_w, in_nor,
                                    toEyeW, shadowFactor);

  frag_color.a = diffuse_albedo.a;

  return litColor;
}
