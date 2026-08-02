module camera;

// Camera parameters for the camera
public struct Camera
{
	public float4x4 inv_view_proj;
	public float4x4 inv_proj;
	public float4x4 prev_view_proj;
	public float4x4 view_proj;
	public float4x4 back_proj;
	public float4x4 view;
	public float3 camera_pos;
	public float3 prev_camera_pos;
};


