import inter_stage.auto_exposure;
import internal.auto_exposure;
import lighting.tonemapping;
import algorithm.color_dither;

layout(set = 0, binding = 0) ConstantBuffer<ExposureResult> exposure_result;
layout(set = 0, binding = 1) Sampler2D<float4> hdr_image;

[[shader("fragment")]]
float4 main(float2 texcoord, float4 fragcoord: SV_Position)
{
	let hdr_color = hdr_image.SampleLevel(texcoord.xy * float2(1.0, -1.0), 0).rgb;
	let exposed_hdr_color = hdr_color * exposure_result.luminance_mult;
	let tonemapped_color = tonemapping::agx<false>(exposed_hdr_color);
	let dithered_color = color_dither::bayer_dither_4x4<8>(tonemapped_color, uint2(floor(fragcoord.xy)));

	return float4(dithered_color, 1.0);
}


