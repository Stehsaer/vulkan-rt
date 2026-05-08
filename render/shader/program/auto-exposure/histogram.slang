import internal.auto_exposure;
import interop.auto_exposure;
import algorithm.id_swizzle;
import sv.compute;

static groupshared uint local_histogram[BINS];

layout(set = 0, binding = 0) ConstantBuffer<ExposureParam> exposure_param;
layout(set = 0, binding = 1) Sampler2D<float4> input_image;
layout(set = 0, binding = 2) RWStructuredBuffer<uint> histogram_buffer;
layout(set = 0, binding = 3) Sampler2D<float> mask_image;

func calc_luminance(color: float3)->float
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

func calc_bin_idx(luminance: float)->uint
{
	let log_lum = log2(luminance);
	let bin_idx = (log_lum - exposure_param.min_log_luminance)
		/ (exposure_param.max_log_luminance - exposure_param.min_log_luminance)
		* BINS;
	return uint(clamp(floor(bin_idx), 0, BINS - 1));
}

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let pixel_coord = sv.global_group_coord.xy * 16 + id_swizzle::swizzle<4, 16>(sv.local_thread_index);
	let texcoord = (float2(pixel_coord) + 0.5) / float2(exposure_param.image_size);

	local_histogram[sv.local_thread_index] = 0;
	GroupMemoryBarrier();

	[[branch]]
	if (all(pixel_coord < exposure_param.image_size))
	{
		let color = input_image.SampleLevel(texcoord, 0).rgb;
		let mask_value_unorm = mask_image.SampleLevel(texcoord, 0);
		let mask_value_uint = uint(mask_value_unorm * 64.0);
		let luminance = calc_luminance(color);
		let bin_idx = calc_bin_idx(luminance);

		InterlockedAdd(local_histogram[bin_idx], mask_value_uint);
	}

	GroupMemoryBarrier();

	InterlockedAdd(histogram_buffer[sv.local_thread_index], local_histogram[sv.local_thread_index]);
}


