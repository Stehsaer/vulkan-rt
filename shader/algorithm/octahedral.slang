module octahedral;

public func oct_encode(in_normal: float3)->float2
{
	var normal = in_normal;
	normal.xy /= dot(1, abs(normal));

	[[flatten]]
	if (normal.z <= 0)
	{
		normal.xy = (1 - abs(normal.yx)) * select(normal.xy >= 0, float2(1, 1), float2(-1, -1));
	}
	return normal.xy;
}

public func oct_decode(oct: float2)->float3
{
	var normal = float3(oct, 1 - dot(1, abs(oct)));

	[[flatten]]
	if (normal.z < 0)
	{
		normal.xy = (1 - abs(normal.yx)) * select(normal.xy >= 0, float2(1, 1), float2(-1, -1));
	}

	return normalize(normal);
}
