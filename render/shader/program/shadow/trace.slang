import model;
import raytrace;

import interop.direct_light;
import interop.camera;
import algorithm.coord;
import algorithm.octahedral;
import algorithm.tbn;

layout(set = 0) ParameterBlock<model::MaterialList> scene_material;
layout(set = 1) ParameterBlock<raytrace::MeshResource> mesh_resource;

layout(set = 2, binding = 0) RaytracingAccelerationStructure tlas;
layout(set = 2, binding = 1) ConstantBuffer<DirectLight> direct_light;
layout(set = 2, binding = 2) ConstantBuffer<Camera> camera;
layout(set = 2, binding = 3) Sampler2D<float> depth_tex;
layout(set = 2, binding = 4) Sampler2D<float4> noise_tex;
layout(set = 2, binding = 5) Sampler2D<float2> motion_vector_tex;
layout(set = 2, binding = 6) RWTexture2D<float2> shadow_tex;
layout(set = 2, binding = 7) Sampler2D<float2> prev_shadow_tex;

static constexpr let DEPTH_BIAS_ULP = 1024;
static constexpr let NOISE_TEX_SIZE = 128;
static constexpr let STATIC_MIX = 0.02;
static constexpr let DYNAMIC_MIX = 0.08;
static constexpr let LOD_LEVEL = 2.0;

struct HitPayload
{
	bool hit;
};

struct PushConstant
{
	uint32_t2 full_size;
	int32_t2 noise_offset;
};

[[vk::push_constant]]
PushConstant constant;

func get_light_direction(noise: float4)->float3
{
	let tbn = find_tbn_matrix(direct_light.direction, float3(1.0, 0.0, 0.0), float3(0.0, 0.0, 1.0));

	let r = sin(noise.x) * direct_light.sin_half_angle;
	let phi = noise.y * float.getPi() * 2;
	let dir = float2(sin(phi), cos(phi));

	return mul(float3(r * dir, 1.0), tbn);
}

func trace_ray(pixcoord: uint2, texcoord: float2, depth: float)->float
{
	let noise = noise_tex.Load(int3((pixcoord + constant.noise_offset) % NOISE_TEX_SIZE, 0));

	let biased_depth = bit_cast<float>(bit_cast<uint>(depth) + DEPTH_BIAS_ULP);
	let ndc = float4(texcoord_to_ndc(texcoord), biased_depth, 1.0);
	let world_pos = w_div(mul(camera.inv_view_projection, ndc));
	let raydesc = RayDesc(world_pos, 0, get_light_direction(noise), 1e10);

	var payload = HitPayload(true);
	TraceRay(
		tlas,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
			| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER
			| RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES,
		0xff,
		0,
		0,
		0,
		raydesc,
		payload
	);

	return select(payload.hit, 0.0, 1.0);
}

func cubic_hermite(x: float4, t: float)->float
{
	let t2 = t * t;
	let t3 = t * t * t;
	let a = dot(x, float4(-0.5, 1.5, -1.5, 0.5));
	let b = dot(x, float4(1.0, -2.5, 2.0, -0.5));
	let c = -x.x / 2.0 + x.z / 2.0;
	let d = x.y;

	return a * t3 + b * t2 + c * t + d;
}

func bicubic_sample(img: Sampler2D<float2>, coord: float2)->float
{
	// Assume unnormalized coordinates

	let texcoord = coord + 0.5;
	let fract_coord = fract(texcoord);
	let sample_texcoord = texcoord - fract_coord;

	let row1 = img.GatherRed(sample_texcoord, int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1));
	let row1_cubic = cubic_hermite(row1, fract_coord.x);

	let row2 = img.GatherRed(sample_texcoord, int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0));
	let row2_cubic = cubic_hermite(row2, fract_coord.x);

	let row3 = img.GatherRed(sample_texcoord, int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1));
	let row3_cubic = cubic_hermite(row3, fract_coord.x);

	let row4 = img.GatherRed(sample_texcoord, int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2));
	let row4_cubic = cubic_hermite(row4, fract_coord.x);

	return cubic_hermite(float4(row1_cubic, row2_cubic, row3_cubic, row4_cubic), fract_coord.y);
}

func temporal_denoise(pixcoord: uint2, curr_shadow: float)->float2
{
	let motion_vector = motion_vector_tex.Load(int3(pixcoord, 0));

	[[branch]]
	if (motion_vector.x > 32768.0)  // Occluded, store directly
		return float2(curr_shadow, (1.0 - curr_shadow) * curr_shadow);

	// let prev = prev_shadow_tex.SampleLevelZero(pixcoord + 0.5 + motion_vector);
	let prev_shadow = bicubic_sample(prev_shadow_tex, pixcoord + 0.5 + motion_vector);

	// Lower history weight according to motion vector strength
	// Reduce incorrect blurring during camera motion
	let motion_strength = length(motion_vector);
	let mix_factor = lerp(STATIC_MIX, DYNAMIC_MIX, saturate(motion_strength * 20));

	let shadow = lerp(prev_shadow, curr_shadow, mix_factor);
	let shadow_variance = shadow * (1.0 - shadow);

	return float2(shadow, shadow_variance);
}

[[shader("raygeneration")]]
void raygen_main()
{
	let pixcoord = DispatchRaysIndex().xy;
	let texcoord = float2(pixcoord * 2 + 0.5) / float2(constant.full_size);

	let depth = depth_tex.Load(int3(pixcoord, 0));

	[[branch]]
	if (depth == 0.0) return;

	let curr_shadow = trace_ray(pixcoord, texcoord, depth);
	let result = temporal_denoise(pixcoord, curr_shadow);

	shadow_tex.Store(pixcoord, result * float2(1.0, 4.0));
}

[[shader("anyhit")]]
void anyhit_main(inout HitPayload payload, BuiltInTriangleIntersectionAttributes intersect_attr)
{
	let hit_info = mesh_resource.get_texcoord_hitinfo(intersect_attr);
	let material_info = scene_material.get_material(hit_info.material_index);
	let albedo_tex = scene_material.textures[material_info.texture_index.albedo];
	let alpha_cutoff = material_info.param.alpha_cutoff;
	let alpha =
		albedo_tex.SampleLevel(hit_info.texcoord, LOD_LEVEL).a * material_info.param.base_color_factor.a;

	if (alpha_cutoff > alpha)
		IgnoreHit();
	else
		AcceptHitAndEndSearch();
}

[[shader("miss")]]
void miss_main(inout HitPayload payload)
{
	payload.hit = false;
}


