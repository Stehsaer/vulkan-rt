import inter_stage.auto_exposure;
import interop.auto_exposure;
import internal.auto_exposure;

layout(set = 0, binding = 0) ConstantBuffer<ExposureParam> exposure_param;
layout(set = 0, binding = 1) RWStructuredBuffer<uint> histogram_buffer;
layout(set = 0, binding = 2) RWStructuredBuffer<ExposureResult> exposure_result;
layout(set = 0, binding = 3) StructuredBuffer<ExposureFrame> last_frame;
layout(set = 0, binding = 4) RWStructuredBuffer<ExposureFrame> current_frame;

static groupshared uint bin_cumsum[BINS];

static const float BRIGHT_CLAMP_THRES = 0.98;
static const float DARK_WEIGHT = 0.3;
static const float BRIGHT_WEIGHT = 1.0;

[[shader("compute"), numthreads(32, 1, 1), WaveSize(32)]]
void main(uint thread_id: SV_GroupIndex)
{
	/* Percentile reduction */

	var total_sum = 0;

	[[unroll(BINS / 32)]]
	for (uint i = 0; i < BINS / 32; i++)
	{
		let idx = i * 32 + thread_id;
		let bin_weight = histogram_buffer[idx];

		let local_cumsum = WavePrefixSum(bin_weight);
		let local_sum = WaveActiveSum(bin_weight);
		let bin_center_sum = total_sum + local_cumsum + bin_weight / 2;

		total_sum += local_sum;
		bin_cumsum[idx] = bin_center_sum;
	}

	GroupMemoryBarrierWithWaveSync();

	/* Local reduction */

	let bins_per_thread = BINS / 32;
	var value_sum = 0.0;
	var weight_sum = 0.0;

	for (uint i = 0; i < bins_per_thread; i++)
	{
		let bin_idx = thread_id * bins_per_thread + i;

		let log_luminance =
			lerp(exposure_param.min_log_luminance, exposure_param.max_log_luminance, (bin_idx + 0.5) / BINS);
		let luminance = exp2(log_luminance);

		let percentile = float(bin_cumsum[bin_idx]) / float(total_sum);
		let percentile_weight = lerp(DARK_WEIGHT, BRIGHT_WEIGHT, percentile)
			* (1.0 - max(0.0, percentile - BRIGHT_CLAMP_THRES) / (1.0 - BRIGHT_CLAMP_THRES));
		let weight = histogram_buffer[bin_idx] * percentile_weight;

		weight_sum += weight;
		value_sum += weight * luminance;
	}

	GroupMemoryBarrierWithWaveSync();

	/* Global reduction */

	let global_value_sum = WaveActiveSum(value_sum);
	let global_weight_sum = WaveActiveSum(weight_sum);

	[[branch]]
	if (WaveIsFirstLane())
	{
		let curr_mid_luminance = global_value_sum / global_weight_sum;
		let last_mid_luminance = last_frame[0].mid_luminance;
		let mix_ratio = exp(-exposure_param.adaptation_rate * exposure_param.delta_time);
		let lerp_mid_luminance = lerp(curr_mid_luminance, last_mid_luminance, mix_ratio);

		current_frame[0].mid_luminance = lerp_mid_luminance;
		exposure_result[0].luminance_mult = 0.18 / lerp_mid_luminance * exposure_param.exposure_mult;
	}
}


