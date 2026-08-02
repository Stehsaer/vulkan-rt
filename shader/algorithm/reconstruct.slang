module reconstruct;

namespace reconstruct
{
	// Get world position from given matrix
	// - `inv_view_proj`: inverse view-projection matrix
	// - `ndc`: NDC coordinate (-1 ~ 1)
	// - `depth`: projected depth
	[[ForceInline]]
	public func get_world_pos(inv_view_proj: float4x4, ndc: float2, depth: float)->float3
	{
		let x = ndc.x * inv_view_proj[0][0]
			+ ndc.y * inv_view_proj[0][1]
			+ depth * inv_view_proj[0][2]
			+ inv_view_proj[0][3];

		let y = ndc.x * inv_view_proj[1][0]
			+ ndc.y * inv_view_proj[1][1]
			+ depth * inv_view_proj[1][2]
			+ inv_view_proj[1][3];

		let z = ndc.x * inv_view_proj[2][0]
			+ ndc.y * inv_view_proj[2][1]
			+ depth * inv_view_proj[2][2]
			+ inv_view_proj[2][3];

		let w = ndc.x * inv_view_proj[3][0]
			+ ndc.y * inv_view_proj[3][1]
			+ depth * inv_view_proj[3][2]
			+ inv_view_proj[3][3];

		return float3(x, y, z) / w;
	}

	// Get view-space position from inverse projection matrix
	// - `inv_proj`: inverse projection matrix
	// - `ndc`: NDC coordinate (-1 ~ 1)
	// - `depth`: projected depth
	[[ForceInline]]
	public func get_view_pos(inv_proj: float4x4, ndc: float2, depth: float)->float3
	{
		let x = ndc.x * inv_proj[0][0];
		let y = ndc.y * inv_proj[1][1];
		let z = depth * inv_proj[2][2] + inv_proj[2][3];
		let w = depth * inv_proj[3][2] + inv_proj[3][3];
		return float3(x, y, z) / w;
	}
}


