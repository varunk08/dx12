// Vertex shader.
cbuffer per_obj_cbuf : register(b0)
{
  float4x4 world_view_proj;
};

void VS(float3 in_pos : POSITION,
        float3 in_nor : NORMAL,
        float2 in_tex : TEXCOORD,
        out float4 out_pos : SV_POSITION)
{
  out_pos = mul(float4(in_pos, 1.0f), world_view_proj);
}



// Pixel shader.
float4 PS(float4 in_pos : SV_POSITION) : SV_TARGET
{
  return float4 ( 1.0f, 1.0f, 1.0f, 1.0f);
}
