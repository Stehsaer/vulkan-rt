static float2 positions[3] = float2[](float2(0.0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5));

static float3 colors[3] = float3[](float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), float3(0.0, 0.0, 1.0));

struct VertexOut
{
	float4 position : SV_Position;
	float3 color;
};

struct PixelIn
{
	float3 color;
};

[shader("vertex")]
VertexOut main_vertex(uint vertex_id: SV_VertexID)
{
	VertexOut out;
	out.position = float4(positions[vertex_id], 0.0, 1.0);
	out.color = colors[vertex_id];
	return out;
}

[shader("fragment")]
float4 main_fragment(PixelIn input)
{
	return float4(input.color, 1.0);
}
