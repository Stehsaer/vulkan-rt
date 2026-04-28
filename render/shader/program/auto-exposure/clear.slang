import internal.auto_exposure;

layout(set = 0, binding = 0) RWStructuredBuffer<uint> histogram_buffer;

[[shader("compute"), numthreads(BINS, 1, 1)]]
void main(uint thread_id: SV_GroupIndex)
{
	histogram_buffer[thread_id] = 0;
}


