module coord;

/* # NOTE:
 *
 * NDC: (-1~1, -1~1), (0, 0) = center, (1, 1) = Top-right
 * Texcoord: (0~1, 0~1), (0, 0) = Top-left, (1, 1) = Bottom-right
 */

// Transform texcoord to NDC
public func texcoord_to_ndc(texcoord: float2)->float2
{
	return fma(texcoord, float2(2.0, -2.0), float2(-1, 1));
}

// Transform NDC to texcoord
public func ndc_to_texcoord(ndc: float2)->float2
{
	return fma(ndc, float2(0.5, -0.5), float2(0.5));
}

// Perspective division
public func w_div(v: float4)->float3
{
	return v.xyz / v.w;
}

// Barycentric interpolation
public func bary_mix<int L>(a: vector<float, L>, b: vector<float, L>, c: vector<float, L>, coord: float3)
	->vector<float, L>
{
	return a * coord.x + b * coord.y + c * coord.z;
}


