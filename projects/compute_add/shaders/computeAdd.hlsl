
struct Data
{
	float3 v1;
};

StructuredBuffer<Data> sbuf_inputA : register(t0);
StructuredBuffer<Data> sbuf_inputB : register(t1);
RWStructuredBuffer<Data> rw_sbuf_output : register(u0);

// num threads in a thread group
[numthreads(32, 1, 1)]
void CS(int3 dtid : SV_DispatchThreadID)
{
	rw_sbuf_output[dtid.x].v1 = sbuf_inputA[dtid.x].v1 * sbuf_inputB[dtid.x].v1;
}
