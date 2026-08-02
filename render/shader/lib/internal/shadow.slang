module shadow;

import algorithm.coord;
import algorithm.octahedral;
import algorithm.reconstruct;

/*===== COMMON CONSTANT =====*/

public static const let VARIANCE_STORAGE_RATIO = 4.0;

/*===== UTILITY =====*/

[[ForceInline]]
public func pow128(x: float)->float
{
	let x2 = x * x;
	let x4 = x2 * x2;
	let x8 = x4 * x4;
	let x16 = x8 * x8;
	let x32 = x16 * x16;
	let x64 = x32 * x32;
	return x64 * x64;
}

// Weight evaluator for shadow
public struct WeightEvaluator
{
	float3 world_normal;
	float3 view_normal;
	float3 view_pos;

	// Creates a weight evaluator
	// - `inv_proj`: inverse projection matrix
	// - `view`: view matrix
	// - `full_extent`: full extent of rendering
	// - `pixcoord`: half-res pixcoord
	// - `depth`: projected depth of reference pixel
	// - `encoded_normal`: encoded world normal of reference pixel
	//
	// **Note**: `depth` will be used first, then `encoded_normal`.
	//
	public static func from(
		inv_proj: float4x4,
		view: float4x4,
		full_extent: uint2,
		pixcoord: uint2,
		depth: float,
		encoded_normal: float2
	)
		->WeightEvaluator
	{
		let ndc = texcoord_to_ndc((float2(pixcoord) * 2 + 0.5) / full_extent);
		let view_pos = reconstruct::get_view_pos(inv_proj, ndc, depth);

		let world_normal = oct_decode(encoded_normal);
		let view_normal = mul(view, float4(world_normal, 0.0)).xyz;

		var evaluator : WeightEvaluator;
		evaluator.world_normal = world_normal;
		evaluator.view_normal = view_normal;
		evaluator.view_pos = view_pos;

		return evaluator;
	}

	// Evaluate weight for a target
	// - `inv_proj`: inverse projection matrix
	// - `full_extent`: full extent of rendering
	// - `pixcoord`: half-res pixcoord
	// - `depth`: projected depth of target pixel
	// - `encoded_normal`: encoded world normal of target pixel
	//
	// **Note**: `depth` will be used first, then `encoded_normal`
	//
	public func eval(
		inv_proj: float4x4,
		full_extent: uint2,
		pixcoord: uint2,
		depth: float,
		encoded_normal: float2
	)
		->float
	{
		let ndc = texcoord_to_ndc((float2(pixcoord) * 2 + 0.5) / full_extent);
		let view_pos = reconstruct::get_view_pos(inv_proj, ndc, depth);
		let view_dir = normalize(view_pos - this.view_pos);
		let depth_weight = 1.0 - abs(dot(view_dir, this.view_normal));

		let world_normal = oct_decode(encoded_normal);
		let normal_weight = max(0.0, dot(world_normal, this.world_normal));

		return exp2(16 * log2(depth_weight) + 128 * log2(normal_weight));
	}
};


