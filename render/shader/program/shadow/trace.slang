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
layout(set = 2, binding = 4) RWTexture2D<float> shadow_tex;
layout(set = 2, binding = 5) Sampler2D<float4> noise_tex;

static const float TMIN_BIAS_BASE = 0.001;
static const float DYNAMIC_BIAS_FACTOR = 0.001;
static const uint NOISE_TEX_SIZE = 128;

static const float LOD_LEVEL = 2;

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

[[shader("raygeneration")]]
void raygen_main()
{
	let pixcoord = DispatchRaysIndex().xy;
	let texcoord = float2(pixcoord * 2) / float2(constant.full_size);

	let depth = depth_tex.Load(int3(pixcoord, 0));

	[[branch]]
	if (depth == 0.0) return;

	let noise = noise_tex.Load(int3((pixcoord + constant.noise_offset) % NOISE_TEX_SIZE, 0));

	let world_pos = w_div(mul(camera.inv_view_projection, float4(texcoord_to_ndc(texcoord), depth, 1.0)));
	let distance = distance(camera.camera_pos, world_pos);

	let bias = TMIN_BIAS_BASE + DYNAMIC_BIAS_FACTOR * pow(distance, 1.5);
	let raydesc = RayDesc(world_pos, bias, get_light_direction(noise), 1e10);

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

	shadow_tex.Store(pixcoord, select(payload.hit, 0.0, 1.0));
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


