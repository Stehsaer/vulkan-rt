import model;
import interop.indirect_drawcall;
import interop.primitive_drawcall;

[[vk::push_constant]]
uint32_t drawcall_count;

layout(set = 0, binding = 0) RWStructuredBuffer<IndirectDrawcall> indirect_entries;
layout(set = 0, binding = 1) StructuredBuffer<PrimitiveDrawcall> drawcalls;
layout(set = 0, binding = 2) StructuredBuffer<float4x4> node_transforms;
layout(set = 0, binding = 3) StructuredBuffer<model::PrimitiveAttribute> primitive_attrs;

[[shader("compute"), numthreads(64, 1, 1)]]
void main(uint global_invocation_id: SV_DispatchThreadID)
{
	if (global_invocation_id >= drawcall_count) return;

	let drawcall = drawcalls[global_invocation_id];

	let node_transform = node_transforms[drawcall.node_index];  // Unused, for culling
	let primitive_attr = primitive_attrs[drawcall.primitive_index];

	var visible = true;  // TODO: culling

	indirect_entries[global_invocation_id] =
		IndirectDrawcall::from(primitive_attr, drawcall, visible, global_invocation_id);
}


