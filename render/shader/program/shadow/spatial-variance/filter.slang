/*===== IMPORT =====*/

import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import interop.camera;
import internal.shadow;
import sv.compute;

/*===== STRUCT =====*/

struct Extent
{
	uint32_t2 half;
	uint32_t2 full;
};

/*===== I/O =====*/

// IMPORTANT: All samplers use:
// - Unnormalized coordinates
// - Nearest filter
// - Clamp to edge

Sampler2D<float> input_stddev_tex;     // Input stddev, stored as `(2 * stddev)`
RWTexture2D<float> output_stddev_tex;  // Output stddev, stored as `(2 * stddev)`
Sampler2D<float> depth_tex;            // Depth reference image
Sampler2D<float2> normal_tex;          // Normal reference image, octahedral encoded
ConstantBuffer<Camera> camera;         // Camera parameters

[[vk::push_constant]]
Extent extent;

/*===== CONSTANT =====*/

static const let EPS = 1e-3;
static const let RADIUS = 1;
static const let STRIDE = 4;

/*===== PROGRAM =====*/
// Compute power-mean of neighbor spatial variance where higher variance has higher weight

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let pixcoord : int2 = swizzle_t4_b16(sv.global_thread_coord.xy);
	if (any(pixcoord >= extent.half)) return;

	let ref_depth = depth_tex.Load(int3(pixcoord, 0));
	if (ref_depth == 0.0)
	{
		output_stddev_tex.Store(pixcoord, 0);
		return;
	}

	let weight_evaluator = WeightEvaluator::from(
		camera.inv_proj,
		camera.view,
		extent.full,
		pixcoord,
		ref_depth,
		normal_tex.Load(int3(pixcoord, 0))
	);

	var variance_sum : float = 0;
	var weight_sum : float = 0;

	for (int y = -RADIUS; y <= RADIUS; y++)
	{
		[[unroll]]
		for (int x = -RADIUS; x <= RADIUS; x++)
		{
			let coord = pixcoord + int2(x, y) * STRIDE;

			let depth = depth_tex.SampleLevelZero(coord);
			let encoded_normal = normal_tex.SampleLevelZero(coord);
			let stddev_raw = input_stddev_tex.SampleLevelZero(coord);

			if (depth == 0) continue;
			let weight = x == 0 && y == 0
				? 1.0
				: weight_evaluator.eval(camera.inv_proj, extent.full, coord, depth, encoded_normal);

			let stddev = stddev_raw / 2;
			let variance = stddev * stddev + EPS;
			let variance_pow3 = variance * variance * variance;

			weight_sum += weight;
			variance_sum += variance_pow3 * weight;
		}
	}

	let variance = max(0.0, pow(variance_sum / weight_sum, 1.0 / 3.0) - EPS);
	let stddev = sqrt(variance);
	output_stddev_tex.Store(pixcoord, stddev * 2);
}


