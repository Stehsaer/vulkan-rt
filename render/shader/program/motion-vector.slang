import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import sv.compute;

import interop.camera;

/* NOTE: All textures are half-res */

Sampler2D<float> depth_tex;
Sampler2D<float> prev_depth_tex;
RWTexture2D<float2> motion_vector_tex;
ConstantBuffer<Camera> camera;

struct PushConstant
{
	uint32_t2 half_size;
	uint32_t2 full_size;
};

[[vk::push_constant]]
PushConstant constant;

static constexpr let INVALID_VALUE = 65000.0;
static constexpr let DEPTH_EPSILON = 0.005;

[[numthreads(16, 16, 1)]]
[[shader("compute")]]
func main(sv: compute::ShaderVar)
{
	let pixcoord = swizzle_t4_b16(sv.global_thread_coord.xy);

	// Out-of-bound
	[[branch]]
	if (any(pixcoord >= constant.half_size)) return;

	let curr_depth = depth_tex.Load(int3(pixcoord, 0));

	// Background
	[[branch]]
	if (curr_depth == 0.0)
	{
		motion_vector_tex.Store(pixcoord, INVALID_VALUE);
		return;
	}

	let curr_pixcoord_center = float2(pixcoord * 2 + 0.5);
	let curr_texcoord = curr_pixcoord_center / constant.full_size;
	let curr_ndc = float4(texcoord_to_ndc(curr_texcoord), curr_depth, 1.0);
	let prev_ndc = w_div(mul(camera.back_projection, curr_ndc));

	[[branch]]
	if (any(abs(prev_ndc.xy) >= 1.0))
	{
		motion_vector_tex.Store(pixcoord, INVALID_VALUE);
		return;
	}

	let prev_texcoord = ndc_to_texcoord(prev_ndc.xy);
	let prev_pixcoord_full = prev_texcoord * constant.full_size;
	let prev_pixcoord_half = prev_pixcoord_full / 2.0 + 0.25;
	let prev_actual_depth = prev_depth_tex.SampleLevelZero(prev_pixcoord_half);
	let prev_depth = prev_ndc.z;

	let rel_error = distance(prev_actual_depth, prev_depth) / prev_depth;
	let rel_coord = select(
		rel_error < DEPTH_EPSILON,
		prev_pixcoord_half - pixcoord - 0.5,
		float2(INVALID_VALUE, INVALID_VALUE)
	);

	motion_vector_tex.Store(pixcoord, rel_coord);
}


