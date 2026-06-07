import algorithm.coord;

static const float2 fullscreen_vert[6] = {
	float2(-1.0, -1.0),
	float2(1.0, -1.0),
	float2(-1.0, 1.0),
	float2(-1.0, 1.0),
	float2(1.0, -1.0),
	float2(1.0, 1.0)
};

struct VertexOutput
{
	float4 clip_space_pos : SV_Position;
	float2 texcoord;
};

[[shader("vertex")]]
VertexOutput main(uint vertex_id: SV_VertexID)
{
	VertexOutput output;

	let vert = fullscreen_vert[vertex_id];
	output.clip_space_pos = float4(vert, 0.0, 1.0);
	output.texcoord = ndc_to_texcoord(vert);

	return output;
}


