module tbn;

// Find tangent vector given a vector and two candidate up vectors
// - `vec`: input vector
// - `up1`: candidate up vector 1
// - `up2`: candidate up vector 2
//
// **NOTE**: two up vectors must not be the same and is recommended to stay as vertically as possible
public func find_tangent_vector(vec: float3, up1: float3, up2: float3)->float3
{
	[[flatten]]
	if (abs(dot(vec, up1)) > 0.8)
		return cross(vec, up2);
	else
		return cross(vec, up1);
}

// Find TBN matrix (**row-major**), given a vector and two candidate up vectors
// - `vec`: input vector
// - `up1`: candidate up vector 1
// - `up2`: candidate up vector 2
//
// **NOTE**: two up vectors must not be the same and is recommended to stay as vertically as possible
public func find_tbn_matrix(vec: float3, up1: float3, up2: float3)->float3x3
{
	let tangent = find_tangent_vector(vec, up1, up2);
	let bitangent = cross(vec, tangent);
	return float3x3(tangent, bitangent, vec);
}


