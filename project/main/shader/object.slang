import pbr;
import tonemapping;
import color_dither;

struct VertexIn
{
	[[vk::location(0)]]
	float3 position;

	[[vk::location(1)]]
	float3 normal;

	[[vk::location(2)]]
	float2 texcoord;
}

struct PerFragmentData
{
	[[vk::location(0)]]
	float3 normal;

	[[vk::location(1)]]
	float2 uv;

	[[vk::location(2)]]
	float3 position;
}

struct VertexOut
{
	float4 position : SV_Position;

	PerFragmentData fragment_data;
}

struct FragmentIn
{
	float4 fragcoord : SV_Position;

	PerFragmentData fragment_data;
}

struct FragmentOut
{
	[[vk::location(0)]]
	float4 color;
}

struct Parameters
{
	float4x4 view_projection;
	float3 view_pos;
}

[[vk::push_constant]]
ConstantBuffer<Parameters> camera_info;

[shader("vertex")]
func main_vertex(input: VertexIn)->VertexOut
{
	let world_position = float4(input.position, 1.0);
	let clip_position = mul(camera_info.view_projection, world_position);
	let output = PerFragmentData(normalize(input.normal), input.texcoord, input.position);

	return VertexOut(clip_position, output);
}

[shader("fragment")]
func main_fragment(input: FragmentIn)->FragmentOut
{
	let material = pbr::Material(normalize(input.fragment_data.normal), float3(1.0, 1.0, 1.0), 0.0, 0.3);
	let light = pbr::DirectionalLight(normalize(float3(1.0, 1.0, 1.0)), float3(1.0));

	let view_dir = normalize(camera_info.view_pos - input.fragment_data.position);
	let linear_light = pbr::gltf(light, material, view_dir) + float3(0.01);

	let srgb_color = tonemapping::agx<false>(linear_light);
	let dithered_color = color_dither::bayer_dither_4x4<8>(srgb_color, uint2(input.fragcoord.xy));

	return FragmentOut(float4(dithered_color, 1.0));
}
