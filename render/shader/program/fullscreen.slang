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
	output.clip_space_pos = float4(fullscreen_vert[vertex_id], 0.0, 1.0);
	output.texcoord = (fullscreen_vert[vertex_id] + 1.0) * 0.5;
	return output;
}

