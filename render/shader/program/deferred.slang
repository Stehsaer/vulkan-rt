import model;
import interop.camera;
import interop.indirect_drawcall;
import algorithm.octahedral;

/*===== Descriptors & Constants =====*/

struct VertexData
{
	float2 texcoord;
	float3 normal;
	float4 tangent;
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

	let clip_position = mul(camera.view_projection, mul(transform, float4(vertex.position, 1.0)));
	let normal = normalize(mul(transform, float4(vertex.normal, 0.0)).xyz);
	let tangent = normalize(mul(transform, float4(vertex.tangent.xyz, 0.0)).xyz);

	VertexOutput output;
	output.clip_space_pos = clip_position;
	output.data.normal = normal;
	output.data.texcoord = vertex.texcoord;
	output.data.tangent = float4(tangent, vertex.tangent.w);
	output.data.primitive_id = indirect_drawcall.drawcall.primitive_index;
	return output;
}

/*===== Fragment Shader =====*/

static const float LOD_BIAS = -0.5;

struct FragmentOutput
{
	[[vk::location(0)]]
	float4 albedo;

	[[vk::location(1)]]
	uint32_t normal;

	[[vk::location(2)]]
	float2 pbr;

	[[vk::location(3)]]
	float4 hdr_emission;
};

[[shader("fragment")]]
FragmentOutput main_fragment(
	VertexData vertex,
	bool is_front_face: SV_IsFrontFace,
	float4 fragcoord: SV_Position
)
{
	/*===== Texture Set =====*/

	let primitive_attr = primitive_attributes[vertex.primitive_id];
	let material_info = material_list.get_material(primitive_attr.material_index);
	let texture_set = material_list.get_texture_set(material_info.texture_index);

	/*===== Albedo =====*/

	let albedo =
		texture_set.albedo.SampleBias(vertex.texcoord, LOD_BIAS) * material_info.param.base_color_factor;

	[[branch]]
	if (alpha_mask_enabled)
		if (albedo.a < material_info.param.alpha_cutoff) discard;

	/*===== Normal =====*/

	let vertex_normal = normalize(vertex.normal);

	let normal_map = texture_set.normal.SampleBias(vertex.texcoord, LOD_BIAS).xy * 2.0 - 1.0;
	let local_normal =
		float3(normal_map * material_info.param.normal_scale, sqrt(1.0 - dot(normal_map, normal_map)));
	let tbn_matrix = float3x3(
		vertex.tangent.xyz,
		cross(vertex_normal, vertex.tangent.xyz) * vertex.tangent.w,
		vertex_normal
	);
	var normal = normalize(mul(local_normal, tbn_matrix));

	[[branch]]
	if (double_sided) normal = select(is_front_face, normal, -normal);

	let encoded_normal = oct_encode(normal);
	let packed_normal = packSnorm2x16(encoded_normal);

	/*===== Roughness & Metallic =====*/

	let roughness_metallic = texture_set.orm.SampleBias(vertex.texcoord, LOD_BIAS).gb
		* float2(material_info.param.roughness_factor, material_info.param.metallic_factor);

	/*===== Emission =====*/

	let emission =
		texture_set.emissive.SampleBias(vertex.texcoord, LOD_BIAS).rgb * material_info.param.emissive_factor;

	/*===== Final =====*/

	return FragmentOutput(float4(albedo.rgb, 1.0), packed_normal, roughness_metallic, float4(emission, 0.0));
}


