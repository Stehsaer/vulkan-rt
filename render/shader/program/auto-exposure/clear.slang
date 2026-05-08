import internal.auto_exposure;
import sv.compute;

layout(set = 0, binding = 0) RWStructuredBuffer<uint> histogram_buffer;

[[shader("compute"), numthreads(BINS, 1, 1)]]
func main(sv: compute::ShaderVar)
{
	histogram_buffer[sv.local_thread_index] = 0;
}


