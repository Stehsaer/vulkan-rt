module direct_light;

// Describes a direct, parallel light
public struct DirectLight
{
	public float3 direction;      // Direction of the light
	public float3 light;          // RGB intensity of the light
	public float sin_half_angle;  // Sine of half the angle of the light cone
};


