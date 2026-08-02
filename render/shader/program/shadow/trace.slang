/*===== IMPORT =====*/

import model;
import raytrace;

import interop.direct_light;
import interop.camera;
import internal.shadow;
import algorithm.coord;
import algorithm.octahedral;
import algorithm.tbn;

/*===== STRUCT =====*/

struct HitPayload
{
	bool hit;
};

struct PushConstant
{
	uint32_t2 full_size;
	uint32_t frame_index;
};

/*===== I/O =====*/

// IMPORTANT: All framebuffer-related samplers use:
// - Unnormalized coordinates
// - Nearest filter
// - Clamp to edge

layout(set = 0) ParameterBlock<model::MaterialList> scene_material;
layout(set = 1) ParameterBlock<raytrace::MeshResource> mesh_resource;

layout(set = 2, binding = 0) RaytracingAccelerationStructure tlas;
layout(set = 2, binding = 1) ConstantBuffer<DirectLight> direct_light;
layout(set = 2, binding = 2) ConstantBuffer<Camera> camera;
layout(set = 2, binding = 3) Sampler2D<float> depth_tex;
layout(set = 2, binding = 4) Sampler3D<float2> noise_tex;
layout(set = 2, binding = 5) RWTexture2D<float> shadow_tex;

[[vk::push_constant]]
PushConstant constant;

/*===== CONSTANT =====*/

static const let DEPTH_BIAS_ULP = 1024;
static const let NOISE_TEX_SIZE = 128;
static const let LINEAR_LOD_LEVEL = 2.0;

/*===== UTILITY =====*/

func get_light_direction(noise: float2)->float3
{
	let tbn = find_tbn_matrix(direct_light.direction, float3(1.0, 0.0, 0.0), float3(0.0, 0.0, 1.0));

	let r = sqrt(noise.x) * direct_light.sin_half_angle;
	let phi = noise.y * float.getPi() * 2;
	let dir = float2(sin(phi), cos(phi));

	return mul(float3(r * dir, 1.0), tbn);
}

func trace_ray(pixcoord: uint2, texcoord: float2, depth: float)->float
{
	let offset = constant.frame_index / 64 * uint2(7, 9) % NOISE_TEX_SIZE;
	let noise = noise_tex.Load(int4((pixcoord + offset) % NOISE_TEX_SIZE, constant.frame_index % 64, 0));
	let biased_depth = bit_cast<float>(bit_cast<uint>(depth) + DEPTH_BIAS_ULP);
	let ndc = float4(texcoord_to_ndc(texcoord), biased_depth, 1.0);
	let world_pos = w_div(mul(camera.inv_view_proj, ndc));
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

/*===== PROGRAM =====*/

[[shader("raygeneration")]]
void raygen_main()
{
	let pixcoord = DispatchRaysIndex().xy;
	let texcoord = float2(pixcoord * 2 + 0.5) / float2(constant.full_size);

	let depth = depth_tex.Load(int3(pixcoord, 0));

	[[branch]]
	if (depth == 0.0)
	{
		shadow_tex.Store(pixcoord, 0);
		return;
	}

	let curr_shadow = trace_ray(pixcoord, texcoord, depth);
	shadow_tex.Store(pixcoord, curr_shadow);
}

[[shader("anyhit")]]
void anyhit_main(inout HitPayload payload, BuiltInTriangleIntersectionAttributes intersect_attr)
{
	let hit_info = mesh_resource.get_texcoord_hitinfo(intersect_attr);
	let material_info = scene_material.get_material(hit_info.material_index);
	let albedo_tex = scene_material.textures[material_info.texture_index.albedo];
	let alpha_cutoff = material_info.param.alpha_cutoff;

	// Use mipmap 0 for nearest filtered albedo
	let mipmap_level =
		select(material_info.sample_mode.albedo.mag_filter == model::Filter::Nearest, 0.0, LINEAR_LOD_LEVEL);

	let alpha =
		albedo_tex.SampleLevel(hit_info.texcoord, mipmap_level).a * material_info.param.base_color_factor.a;

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


