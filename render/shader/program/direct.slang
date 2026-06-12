import interop.camera;
import interop.direct_light;

import lighting.pbr;

import algorithm.octahedral;
import algorithm.coord;

layout(set = 0, binding = 0) Sampler2D<float4> albedo_tex;
layout(set = 0, binding = 1) Sampler2D<uint32_t> normal_tex;
layout(set = 0, binding = 2) Sampler2D<float2> pbr_tex;
layout(set = 0, binding = 3) Sampler2D<float> depth_tex;
layout(set = 0, binding = 4) Sampler2D<float> shadow_tex;
layout(set = 0, binding = 5) ConstantBuffer<Camera> camera;
layout(set = 0, binding = 6) ConstantBuffer<DirectLight> light;

static const float AMBIENT = 0.03;

[[shader("fragment")]]
float4 main(float2 texcoord, float4 fragcoord: SV_Position)
{
	/*===== Texture Access =====*/

	let albedo = albedo_tex.SampleLevelZero(texcoord);

	[[branch]]
	if (albedo.a < 0.5) discard;

	let packed_normal = normal_tex.SampleLevelZero(texcoord);
	let roughness_metallic = pbr_tex.SampleLevelZero(texcoord);
	let depth = depth_tex.SampleLevelZero(texcoord);
	let shadow = shadow_tex.SampleLevelZero(texcoord);

	/*===== Normal Unpack =====*/

	let encoded_normal = unpackSnorm2x16ToFloat(packed_normal);
	let normal = oct_decode(encoded_normal);

	/*===== Position =====*/

	let ndc = float4(texcoord_to_ndc(texcoord), depth, 1.0);
	let world_pos = w_div(mul(camera.inv_view_projection, ndc));

	let view_dir = normalize(camera.camera_pos - world_pos);

	/*===== Lighting Compute =====*/

	let material = pbr::Material(normal, albedo.rgb, roughness_metallic.g, roughness_metallic.r);
	let light = pbr::DirectionalLight(light.direction, light.light);

	let ambient_color = albedo.rgb * AMBIENT * (1.0 - lerp(0.04, albedo.rgb, roughness_metallic.g));
	let color = pbr::gltf(light, material, view_dir);

	return float4(color * shadow + ambient_color, 1.0);
}


