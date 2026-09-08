/*===== IMPORT =====*/

import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import algorithm.reconstruct;
import interop.camera;
import internal.shadow;
import sv.compute;

/*===== STRUCT =====*/

struct Extent
{
	uint32_t2 half;
	uint32_t2 full;
};

/*==== I/O =====*/

// IMPORTANT: All samplers use:
// - Unnormalized coordinates
// - Nearest filter
// - Clamp to edge

Sampler2D<float> input_tex;     // Input shadow sample, 0 = No Light, 1 = Have Light
RWTexture2D<float> mean_tex;    // Output mean, stored as-is
RWTexture2D<float> stddev_tex;  // Output stddev, stored as `(2 * stddev)`
Sampler2D<float> depth_tex;     // Depth reference image
Sampler2D<float2> normal_tex;   // Normal reference image, octahedral encoded
ConstantBuffer<Camera> camera;  // Camera parameters

[[vk::push_constant]]
Extent extent;

/*===== CONSTANT =====*/

static const let RADIUS = 2;

// Variance boost
static const let VARIANCE_BOOST_END = 4.0;
static const let VARIANCE_BOOST_START = 6.0;

/*===== PROGRAM =====*/
// Compute local mean and variance in a given radius on current frame

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let pixcoord : int2 = swizzle_t4_b16(sv.global_thread_coord.xy);
	if (any(pixcoord >= extent.half)) return;

	let ref_depth = depth_tex.Load(int3(pixcoord, 0));
	if (ref_depth == 0.0)
	{
		mean_tex.Store(pixcoord, 0);
		stddev_tex.Store(pixcoord, 0);
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

	var total_weight : float = 1;
	var total : float = input_tex.SampleLevelZero(pixcoord);
	var total_squared : float = total * total;

	for (int y = -RADIUS; y <= RADIUS; y++)
	{
		for (int x = -RADIUS; x <= RADIUS; x++)
		{
			if (x == 0 && y == 0) continue;

			let coord = pixcoord + int2(x, y);

			let depth = depth_tex.SampleLevelZero(coord);
			let encoded_normal = normal_tex.SampleLevelZero(coord);
			let sample = input_tex.SampleLevelZero(coord);

			if (depth == 0) continue;

			let weight = weight_evaluator.eval(camera.inv_proj, extent.full, coord, depth, encoded_normal);

			total_weight += weight;
			total += sample * weight;
			total_squared += sample * sample * weight;
		}
	}

	let mean = total / total_weight;

	// Boost variance when too few valid neighbors
	let variance_boost =
		saturate((total_weight - VARIANCE_BOOST_START) / (VARIANCE_BOOST_END - VARIANCE_BOOST_START));
	let variance = total_squared / total_weight - mean * mean + variance_boost;

	mean_tex.Store(pixcoord, mean);
	stddev_tex.Store(pixcoord, sqrt(variance) * 2);
}


