import sv.compute;

Sampler2D<float4> src_albedo_tex;
Sampler2D<float2> src_normal_tex;
Sampler2D<float2> src_geom_normal_tex;
Sampler2D<float2> src_smooth_normal_tex;
Sampler2D<float2> src_pbr_tex;
Sampler2D<float> src_depth_tex;
RWTexture2D<float4> dst_albedo_tex;
RWTexture2D<float2> dst_normal_tex;
RWTexture2D<float2> dst_geom_normal_tex;
RWTexture2D<float2> dst_smooth_normal_tex;
RWTexture2D<float2> dst_pbr_tex;
RWTexture2D<float> dst_depth_tex;

[[vk::push_constant]]
uint32_t2 dst_size;

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	if (any(sv.global_thread_coord.xy >= dst_size)) return;

	let src_load_coord = sv.global_thread_coord.xy * 2 + 0.5;
	let dst_store_coord = sv.global_thread_coord.xy;

	let albedo = src_albedo_tex.SampleLevelZero(src_load_coord);
	let normal = src_normal_tex.SampleLevelZero(src_load_coord);
	let geom_normal = src_geom_normal_tex.SampleLevelZero(src_load_coord);
	let smooth_normal = src_smooth_normal_tex.SampleLevelZero(src_load_coord);
	let pbr = src_pbr_tex.SampleLevelZero(src_load_coord);
	let depth = src_depth_tex.SampleLevelZero(src_load_coord);

	let srgb_albedo = pow(albedo, 1 / 2.2);

	dst_albedo_tex.Store(dst_store_coord, srgb_albedo);
	dst_normal_tex.Store(dst_store_coord, normal);
	dst_geom_normal_tex.Store(dst_store_coord, geom_normal);
	dst_smooth_normal_tex.Store(dst_store_coord, smooth_normal);
	dst_pbr_tex.Store(dst_store_coord, pbr);
	dst_depth_tex.Store(dst_store_coord, depth);
}


