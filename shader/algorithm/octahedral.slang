module octahedral;

[[ForceInline]]
public func oct_encode(in_normal: float3)->float2
{
	let abs_normal = abs(in_normal);
	let coord = in_normal.xy / (abs_normal.x + abs_normal.y + abs_normal.z);
	return in_normal.z <= 0 ? copysign(1 - abs(coord.yx), coord.xy) : coord;
}

[[ForceInline]]
public func oct_decode(oct: float2)->float3
{
	let sign = 1 - abs(oct.x) - abs(oct.y);
	let xy = sign < 0 ? copysign(1 - abs(oct.yx), oct) : oct;
	return normalize(float3(xy, sign));
}


