import model;
import interop.camera;
import interop.indirect_drawcall;
import algorithm.octahedral;

/*===== Descriptors & Constants =====*/

struct VertexData
{
	[[vk::location(0)]]
	float4 slot1;  // XYZ: world_pos, W: texcoord.x

	[[vk::location(1)]]
	float4 slot2;  // XYZ: normal, W: texcoord.y

	[[vk::location(2)]]
	float4 tangent;

	[[vk::location(3)]]
	nointerpolation uint primitive_id;
};

layout(set = 0) ParameterBlock<model::MaterialList> material_list;

layout(set = 1, binding = 0) StructuredBuffer<model::PrimitiveAttribute> primitive_attributes;
layout(set = 1, binding = 1) StructuredBuffer<IndirectDrawcall> indirect_drawcalls;
layout(set = 1, binding = 2) StructuredBuffer<float4x4> node_transforms;
layout(set = 1, binding = 3) ConstantBuffer<Camera> camera;

[[vk::constant_id(0)]]
const bool alpha_mask_enabled = false;

[[vk::constant_id(1)]]
const bool double_sided = false;

/*===== Vertex Shader =====*/

struct VertexOutput
{
	float4 clip_space_pos : SV_Position;
	VertexData data;
};

[[shader("vertex")]]
VertexOutput main_vertex(model::Vertex vertex, uint instance_id: SV_StartInstanceLocation)
{
	let indirect_drawcall = indirect_drawcalls[instance_id];
	let transform = node_transforms[indirect_drawcall.drawcall.node_index];

	let world_pos = mul(transform, float4(vertex.position, 1.0));
	let clip_position = mul(camera.view_proj, world_pos);
	let normal = normalize(mul(transform, float4(vertex.normal, 0.0)).xyz);
	let tangent = normalize(mul(transform, float4(vertex.tangent.xyz, 0.0)).xyz);

	let slot1 = float4(world_pos.xyz / world_pos.w, vertex.texcoord.x);
	let slot2 = float4(normal, vertex.texcoord.y);

	let vertex_data = VertexData(
		slot1,
		slot2,
		float4(tangent, vertex.tangent.w),
		indirect_drawcall.drawcall.primitive_index
	);

	return VertexOutput(clip_position, vertex_data);
}

/*===== Fragment Shader =====*/

static const float LOD_BIAS = -0.5;

struct FragmentOutput
{
	[[vk::location(0)]]
	float4 albedo;

	[[vk::location(1)]]
	float2 normal;

	[[vk::location(2)]]
	float2 geom_normal;

	[[vk::location(3)]]
	float2 smooth_normal;

	[[vk::location(4)]]
	float2 pbr;

	[[vk::location(5)]]
	float4 hdr_emission;
};

[[shader("fragment")]]
FragmentOutput main_fragment(VertexData vertex, bool is_front_face: SV_IsFrontFace)
{
	let texcoord = float2(vertex.slot1.w, vertex.slot2.w);

	/*===== Texture Set =====*/

	let primitive_attr = primitive_attributes[vertex.primitive_id];
	let material_info = material_list.get_material(primitive_attr.material_index);
	let texture_set = material_list.get_texture_set(material_info.texture_index);

	/*===== Albedo =====*/

	let albedo = texture_set.albedo.SampleBias(texcoord, LOD_BIAS) * material_info.param.base_color_factor;

	[[branch]]
	if (alpha_mask_enabled)
		if (albedo.a < material_info.param.alpha_cutoff) discard;

	/*===== Normal =====*/

	let normal_map_raw = texture_set.normal.SampleBias(texcoord, LOD_BIAS);
	let vertex_normal = normalize(vertex.slot2.xyz);
	let tbn_matrix = float3x3(
		vertex.tangent.xyz,
		cross(vertex_normal, vertex.tangent.xyz) * vertex.tangent.w,
		vertex_normal
	);
	let normal_map = normal_map_raw.xy * 2.0 - 1.0;
	let local_normal =
		float3(normal_map * material_info.param.normal_scale, sqrt(1.0 - dot(normal_map, normal_map)));
	var normal = normalize(mul(local_normal, tbn_matrix));

	let position = vertex.slot1.xyz;
	var smooth_normal = vertex_normal;
	var geom_normal = cross(ddy(position), ddx(position));

	[[branch]]
	if (double_sided)
	{
		normal = select(is_front_face, normal, -normal);
		geom_normal = select(is_front_face, geom_normal, -geom_normal);
	}

	let encoded_normal = oct_encode(normal);
	let encoded_geom_normal = oct_encode(geom_normal);
	let encoded_smooth_normal = oct_encode(smooth_normal);

	/*===== Roughness & Metallic =====*/

	let roughness_metallic = texture_set.orm.SampleBias(texcoord, LOD_BIAS).gb
		* float2(material_info.param.roughness_factor, material_info.param.metallic_factor);

	/*===== Emission =====*/

	let emission =
		texture_set.emissive.SampleBias(texcoord, LOD_BIAS).rgb * material_info.param.emissive_factor;

	/*===== Final =====*/

	return FragmentOutput(
		float4(albedo.rgb, 1.0),
		encoded_normal,
		encoded_geom_normal,
		encoded_smooth_normal,
		roughness_metallic,
		float4(emission, 0.0)
	);
}


