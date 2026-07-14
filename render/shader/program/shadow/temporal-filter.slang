/*===== IMPORT =====*/

import algorithm.bicubic;
import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import interop.camera;
import internal.shadow;
import sv.compute;

/*===== I/O =====*/

Sampler2D<float> prev_history_tex;    // Previous shadow visibility
Sampler2D<float> curr_shadow_tex;     // Current *binary* shadow visibility
Sampler2D<float> mean_tex;            // Mean (spatial)
Sampler2D<float> stddev_tex;          // Stddev (spatial). Stored as `(2 * stddev)`
Sampler2D<float2> motion_vector_tex;  // Motion vector. Absolute value greater than 32768 considered invalid

RWTexture2D<float2> denoise_tex;      // Output texture for denoising. R = mean, G = `variance * 4`
RWTexture2D<float> curr_history_tex;  // Current shadow visibility, for use by next frame

[[vk::push_constant]]
uint32_t2 half_extent;

/*===== CONSTANT =====*/

static const let CLAMP_SIGMA = 2.0;
static const let REDUCE_SIGMA = 1.0;
static const let SIGMA_OFFSET = 0.03;

static const let STATIC_MIX = 0.01;
static const let DYNAMIC_MIX = 0.08;

static const let MOTION_LENGTH_MUL = 20.0;

/*===== Steps =====*/

func compute_temporal(pixcoord: uint2)->float2
{
	let motion_vector = motion_vector_tex.Load(int3(pixcoord, 0));
	let shadow = curr_shadow_tex.Load(int3(pixcoord, 0));
	let mean = mean_tex.Load(int3(pixcoord, 0));
	let stddev = stddev_tex.Load(int3(pixcoord, 0)) / 2 + SIGMA_OFFSET;

	[[branch]]
	if (motion_vector.x > 32768) return float2(mean, stddev * stddev);

	let prev_shadow = bicubic::sample(prev_history_tex, pixcoord + 0.5 + motion_vector);

	let sigma = distance(mean, prev_shadow) / stddev;
	let history_clamp_factor = saturate((sigma - REDUCE_SIGMA) / (CLAMP_SIGMA - REDUCE_SIGMA));

	let motion_strength = length(motion_vector);
	let base_mix_factor = lerp(STATIC_MIX, DYNAMIC_MIX, saturate(motion_strength * MOTION_LENGTH_MUL));
	let mix_factor = lerp(base_mix_factor, 1.0, history_clamp_factor);

	let acc_shadow = lerp(prev_shadow, shadow, mix_factor);
	let acc_shadow_variance = acc_shadow * (1.0 - acc_shadow);

	return float2(acc_shadow, acc_shadow_variance);
}

/*===== PROGRAM =====*/
// Temporally accumulates visibility data, meanwhile performs varianced-based clamping on history value

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let pixcoord = swizzle_t4_b16(sv.global_thread_coord.xy);
	if (any(pixcoord >= half_extent)) return;

	let result = compute_temporal(pixcoord);

	denoise_tex.Store(pixcoord, float2(result.x, result.y * VARIANCE_STORAGE_RATIO));
	curr_history_tex.Store(pixcoord, result.x);
}


