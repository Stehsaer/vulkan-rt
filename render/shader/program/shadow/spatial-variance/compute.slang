/*===== IMPORT =====*/

import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import interop.camera;
import internal.shadow;
import sv.compute;

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
uint32_t2 half_extent;

/*===== CONSTANT =====*/

static const let EPS = 1e-3;
static const let RADIUS = 2;

// Sigmas, taken from SVGF paper (Section 4.4)
static const let SIGMA_Z = 1.0;
static const let SIGMA_N = 128.0;

// Variance boost
static const let VARIANCE_BOOST_END = 4.0;
static const let VARIANCE_BOOST_START = 6.0;

/*===== PROGRAM =====*/
// Compute local mean and variance in a given radius on current frame

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let pixcoord : int2 = swizzle_t4_b16(sv.global_thread_coord.xy);
	if (any(pixcoord >= half_extent)) return;

	let ref_depth = depth_tex.Load(int3(pixcoord, 0));
	if (ref_depth == 0.0)
	{
		mean_tex.Store(pixcoord, 0);
		stddev_tex.Store(pixcoord, 0);
		return;
	}

	let ref_normal = oct_decode(normal_tex.Load(int3(pixcoord, 0)));

	let view_normal = mul(camera.view, float4(ref_normal, 0.0)).xy;
	let depth_gradient = view_normal / sqrt(1.0 - view_normal * view_normal);

	var total_weight : float = 0;
	var total : float = 0;
	var total_squared : float = 0;

	for (int y = -RADIUS; y <= RADIUS; y++)
	{
		[[unroll]]
		for (int x = -RADIUS; x <= RADIUS; x++)
		{
			let coord = pixcoord + int2(x, y);

			let encoded_normal = normal_tex.SampleLevelZero(coord);
			let depth = depth_tex.SampleLevelZero(coord);
			let sample = input_tex.SampleLevelZero(coord);

			let depth_weight =
				exp(-distance(depth, ref_depth) / (SIGMA_Z * dot(depth_gradient, float2(x, -y)) + EPS));
			let normal = oct_decode(encoded_normal);
			let normal_weight = pow128(max(0.0, dot(normal, ref_normal)));
			let weight = normal_weight * depth_weight;

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


