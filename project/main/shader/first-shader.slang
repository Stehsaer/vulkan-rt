struct VertexIn
{
	[vk::location(0)]
	float2 position;

	[vk::location(1)]
	float2 uv;
};

struct VertexOut
{
	float4 position : SV_Position;

	[vk::location(0)]
	float2 uv;
};

struct FragIn
{
	[vk::location(0)]
	float2 uv;
};

struct FragTextures
{
	Sampler2D<float4> texture0;
};

layout(set = 0) ParameterBlock<FragTextures> textures;

[shader("vertex")]
VertexOut main_vertex(VertexIn input)
{
	VertexOut out;
	out.position = float4(input.position, 0.0, 1.0);
	out.uv = input.uv;
	return out;
}

[shader("fragment")]
float4 main_fragment(FragIn input)
{
	return textures.texture0.Sample(input.uv);
}
