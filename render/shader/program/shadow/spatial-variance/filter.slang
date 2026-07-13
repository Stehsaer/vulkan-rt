/*===== IMPORT =====*/

import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import interop.camera;
import internal.shadow;
import sv.compute;

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
uint32_t2 half_extent;

/*===== CONSTANT =====*/

static constexpr let EPS = 1e-3;
static constexpr let RADIUS = 2;
static constexpr let STRIDE = 4;
static constexpr let GAUS_WEIGHT : float[3] = { 3.0 / 8.0, 1.0 / 4.0, 1.0 / 10.0 };

// Sigmas, taken from SVGF paper (Section 4.4)
static constexpr let SIGMA_Z = 1.0;
static constexpr let SIGMA_N = 128.0;

/*===== PROGRAM =====*/
// Compute power-mean of neighbor spatial variance where higher variance has higher weight

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let pixcoord : int2 = swizzle_t4_b16(sv.global_thread_coord.xy);
	if (any(pixcoord >= half_extent)) return;

	let ref_depth = depth_tex.Load(int3(pixcoord, 0));
	if (ref_depth == 0.0)
	{
		output_stddev_tex.Store(pixcoord, 0);
		return;
	}

	let ref_normal = oct_decode(normal_tex.Load(int3(pixcoord, 0)));

	let view_normal = mul(camera.view, float4(ref_normal, 0.0)).xy;
	let depth_gradient = view_normal / sqrt(1.0 - view_normal * view_normal);

	var variance_sum : float = 0;
	var weight_sum : float = 0;

	for (int y = -RADIUS; y <= RADIUS; y++)
	{
		[[unroll]]
		for (int x = -RADIUS; x <= RADIUS; x++)
		{
			let coord = pixcoord + int2(x, y) * STRIDE;

			let encoded_normal = normal_tex.SampleLevelZero(coord);
			let depth = depth_tex.SampleLevelZero(coord);
			let stddev_raw = input_stddev_tex.SampleLevelZero(coord);

			let normal = oct_decode(encoded_normal);
			let stddev = stddev_raw / 2;
			let variance = stddev * stddev + EPS;

			let normal_weight = pow(max(0.0, dot(normal, ref_normal)), SIGMA_N);
			let depth_weight =
				exp(-distance(depth, ref_depth) / (SIGMA_Z * dot(depth_gradient, float2(x, -y)) + EPS));
			let weight = normal_weight * depth_weight * GAUS_WEIGHT[abs(x)] * GAUS_WEIGHT[abs(y)];

			let variance_pow3 = variance * variance * variance;

			weight_sum += weight;
			variance_sum += variance_pow3 * weight;
		}
	}

	let variance = max(0.0, pow(variance_sum / weight_sum, 1.0 / 3.0) - EPS);
	let stddev = sqrt(variance);
	output_stddev_tex.Store(pixcoord, stddev * 2);
}


