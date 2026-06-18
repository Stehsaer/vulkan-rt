import model;
import interop.indirect_drawcall;
import interop.primitive_drawcall;
import interop.camera;
import sv.compute;

[[vk::push_constant]]
uint32_t drawcall_count;

layout(set = 0, binding = 0) RWStructuredBuffer<IndirectDrawcall> indirect_entries;
layout(set = 0, binding = 1) StructuredBuffer<PrimitiveDrawcall> drawcalls;
layout(set = 0, binding = 2) StructuredBuffer<float4x4> node_transforms;
layout(set = 0, binding = 3) StructuredBuffer<model::PrimitiveAttribute> primitive_attrs;
layout(set = 0, binding = 4) ConstantBuffer<Camera> camera;

func test_box_plane(plane: float4, min: float3, max: float3)->bool
{
	let positive_vertex = select(plane.xyz >= 0, max, min);
	return dot(plane.xyz, positive_vertex) + plane.w >= 0;
}

func visible(mat: float4x4, min: float3, max: float3)->bool
{
	return test_box_plane(mat[3] + mat[0], min, max)
		&& test_box_plane(mat[3] - mat[0], min, max)
		&& test_box_plane(mat[3] + mat[1], min, max)
		&& test_box_plane(mat[3] - mat[1], min, max)
		&& test_box_plane(mat[3] + mat[2], min, max)
		&& test_box_plane(mat[3] - mat[2], min, max);
}

[[shader("compute"), numthreads(64, 1, 1)]]
func main(sv: compute::ShaderVar)
{
	let idx = sv.global_thread_coord.x;
	if (idx >= drawcall_count) return;

	let drawcall = drawcalls[idx];

	let node_transform = node_transforms[drawcall.node_index];
	let local_to_clip = mul(camera.view_projection, node_transform);
	let primitive_attr = primitive_attrs[drawcall.primitive_index];
	let is_visible = visible(local_to_clip, primitive_attr.aabb_min, primitive_attr.aabb_max);

	indirect_entries[idx] = IndirectDrawcall::from(primitive_attr, drawcall, is_visible, idx);
}


