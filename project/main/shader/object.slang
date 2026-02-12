struct VertexIn
{
	float3 position;
	float3 normal;
	float2 texcoord;
};

struct VertexInfo
{
	[[vk::location(0)]]
	float3 normal;

	[[vk::location(1)]]
	float2 uv;
};

struct VertexOut
{
	float4 position : SV_Position;

	VertexInfo info;
};

[shader("vertex")]
VertexOut main_vertex(VertexIn input, uniform float4x4 view_proj)
{
	const float4 world_position = float4(input.position, 1.0);
	const float4 clip_position = mul(view_proj, world_position);

	return VertexOut(clip_position, VertexInfo(normalize(input.normal), input.texcoord));
}

[shader("fragment")]
float4 main_fragment(VertexInfo input)
{
	const float3 normal = normalize(input.normal);
	const float brightness = max(0.0, dot(normal, normalize(float3(0.0, 1.0, 0.5))));
	const float bottom_brightness = max(0.0, dot(normal, normalize(float3(0.0, -1.0, 1.0))));
	const float cumulative_brightness = brightness * 0.95 + bottom_brightness * 0.025 + 0.025;

	const float4 color = float4(cumulative_brightness.xxx, 1.0);
	return color;
}
