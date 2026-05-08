import model;
import interop.indirect_drawcall;
import interop.primitive_drawcall;
import sv.compute;

[[vk::push_constant]]
uint32_t drawcall_count;

layout(set = 0, binding = 0) RWStructuredBuffer<IndirectDrawcall> indirect_entries;
layout(set = 0, binding = 1) StructuredBuffer<PrimitiveDrawcall> drawcalls;
layout(set = 0, binding = 2) StructuredBuffer<float4x4> node_transforms;
layout(set = 0, binding = 3) StructuredBuffer<model::PrimitiveAttribute> primitive_attrs;

[[shader("compute"), numthreads(64, 1, 1)]]
func main(sv: compute::ShaderVar)
{
	let idx = sv.global_thread_coord.x;
	if (idx >= drawcall_count) return;

	let drawcall = drawcalls[idx];

	let node_transform = node_transforms[drawcall.node_index];  // Unused, for culling
	let primitive_attr = primitive_attrs[drawcall.primitive_index];

	var visible = true;  // TODO: culling

	indirect_entries[idx] = IndirectDrawcall::from(primitive_attr, drawcall, visible, idx);
}


