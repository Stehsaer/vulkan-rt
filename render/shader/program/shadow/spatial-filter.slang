import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import interop.camera;
import sv.compute;

struct Param
{
	uint32_t2 half_extent;
	uint32_t stride;
};

// IMPORTANT: All samplers use:
// - Unnormalized coordinates
// - Nearest filter
// - Clamp to edge

Sampler2D<float2> input_tex;     // Input image,  where: R = luminance, G = variance * 4
RWTexture2D<float2> output_tex;  // Output image, where: R = luminance, G = variance * 4
Sampler2D<float> depth_tex;      // Depth reference image
Sampler2D<float2> normal_tex;    // Normal reference image, octahedral encoded
ConstantBuffer<Camera> camera;

[[vk::push_constant]]
Param param;

static constexpr let RADIUS = 2;

// Gaussian positional weight
static constexpr let GAUS_WEIGHT : float[3] = { 3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0 };

// Sigmas, taken from SVGF paper (Section 4.4)
static constexpr let SIGMA_Z = 1.0;
static constexpr let SIGMA_N = 128.0;
static constexpr let SIGMA_L = 4.0;
static constexpr let EPS = 1e-3;

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let pixcoord = swizzle_t4_b16(sv.global_thread_coord.xy);
	let pixcoord_center = pixcoord + 0.5;

	[[branch]]
	if (any(pixcoord >= param.half_extent)) return;

	/*===== Sample Reference =====*/

	let ref_depth = depth_tex.SampleLevelZero(pixcoord_center);

	[[branch]]
	if (ref_depth == 0)
	{
		output_tex.Store(pixcoord, float2(0.0, 0.0));
		return;
	}

	let ref_normal = oct_decode(normal_tex.SampleLevelZero(pixcoord_center));
	let ref_input = input_tex.SampleLevelZero(pixcoord_center);

	let max_lum = WaveActiveMax(ref_input.x);
	let min_lum = WaveActiveMin(ref_input.x);

	if (max_lum == 0.0 || min_lum == 1.0)
	{
		output_tex.Store(pixcoord, ref_input);
		return;
	}

	let view_normal = mul(camera.view, float4(ref_normal, 0.0)).xy;
	let depth_gradient = view_normal / sqrt(1.0 - view_normal * view_normal);

	/*===== 3x3 Filter Variance =====*/

	let ref_var =
		(dot(input_tex.GatherGreen(pixcoord_center, int2(-1, -1), int2(-1, 1), int2(1, -1), int2(1, 1)), 1.0)
			 * GAUS_WEIGHT[1]
			 * GAUS_WEIGHT[1]
		 + dot(input_tex.GatherGreen(pixcoord_center, int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1)), 1.0)
			 * GAUS_WEIGHT[1]
		 + ref_input.g)
		/ 4.0
		/ 9.0;
	let sqrt_ref_var = sqrt(ref_var);
	let lum_weight_denom = SIGMA_L * sqrt_ref_var + EPS;

	/*===== Filtering =====*/

	var output_sum = float2(0.0);
	var weight_sum = 0.0;

	for (int y = -RADIUS; y <= RADIUS; y++)
	{
		[[unroll]]
		for (int x = -RADIUS; x <= RADIUS; x++)
		{
			let target_coord = pixcoord_center + float2(x, y) * param.stride;

			let target_encoded_normal = normal_tex.SampleLevelZero(target_coord);
			let target_depth = depth_tex.SampleLevelZero(target_coord);
			let target_input = input_tex.SampleLevelZero(target_coord);

			let target_normal = oct_decode(target_encoded_normal);

			var weight = GAUS_WEIGHT[abs(x)] * GAUS_WEIGHT[abs(y)];            // Gaussian weight
			weight *= pow(max(0.0, dot(target_normal, ref_normal)), SIGMA_N);  // Normal weight

			// Depth weight & Luminance weight
			weight *=
				exp(-distance(target_depth, ref_depth) / (SIGMA_Z * dot(depth_gradient, float2(x, -y)) + EPS)
					- distance(target_input.r, ref_input.r) / lum_weight_denom);

			// Depth mask weight
			weight *= select(target_depth == 0.0, 0.0, 1.0);

			weight_sum += weight;
			output_sum += target_input * float2(weight, weight * weight);
		}
	}

	/*===== Output =====*/

	let result = output_sum / float2(weight_sum, weight_sum * weight_sum);
	output_tex.Store(pixcoord, result);
}


