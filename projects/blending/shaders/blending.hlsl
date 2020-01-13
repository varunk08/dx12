Texture2D diffuse_map : register(t0);
SamplerState linear_wrap_samp : register(s0);

// Vertex shader.
cbuffer per_obj_cbuf : register(b0)
{
  float4x4 world_view_proj;
};

// Material constants.
cbuffer material_cbuf : register(b1)
{
  float4x4 mat_transform;
};

void VS(float3 in_pos : POSITION,
        float3 in_nor : NORMAL,
        float2 in_tex : TEXCOORD,
        out float4 out_pos : SV_POSITION,
        out float2 out_texC : TEXCOORD)
{
  out_pos = mul(float4(in_pos, 1.0f), world_view_proj);
  out_texC = mul(float4(in_tex, 0.0f, 1.0f), mat_transform).xy;
}



// Pixel shader.
float4 PS(float4 in_pos : SV_POSITION,
          float2 in_texC : TEXCOORD) : SV_TARGET
{
  return diffuse_map.Sample(linear_wrap_samp, in_texC);
}
