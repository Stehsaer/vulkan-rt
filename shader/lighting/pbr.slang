module pbr;

namespace pbr
{
	public struct Material
	{
		public float3 normal;
		public float3 albedo;
		public float metallic;
		public float roughness;
	}

	public struct DirectionalLight
	{
		public float3 direction;
		public float3 color;
	}

	func microfacet_dist(n_dot_h: float, alpha: float)->float
	{
		let nominator = alpha * alpha;
		let f = n_dot_h * n_dot_h * (alpha * alpha - 1.0) + 1.0;
		let denominator = float.getPi() * f * f;

		return nominator / denominator;
	}

	func mask_shadowing(vdot: float, alpha: float)->float
	{
		let nominator = lerp(0.0001, 2.0, vdot);
		let alpha2 = alpha * alpha;
		let denominator = vdot + sqrt(alpha2 + (1.0 - alpha2) * vdot * vdot);

		return nominator / denominator;
	}

	public func gltf(light: DirectionalLight, material: Material, view_dir: float3)->float3
	{
		let halfway_dir = normalize(light.direction + view_dir);

		let n_dot_v = max(0.0, abs(dot(view_dir, material.normal)));
		let n_dot_l = max(0.0, dot(material.normal, light.direction));
		let n_dot_h = max(0.0, dot(material.normal, halfway_dir));
		let v_dot_h = max(0.0, dot(view_dir, halfway_dir));

		let c_diff = lerp(material.albedo, float3(0.0), material.metallic);
		let f0 = lerp(float3(0.04), material.albedo, material.metallic);
		let fd90 = 0.5 + 2.0 * v_dot_h * v_dot_h * material.roughness;
		let alpha = material.roughness * material.roughness;

		let F = f0 + (1.0 - f0) * pow(1.0 - v_dot_h, 5.0);
		let light_scatter = lerp(1.0, fd90, pow(1.0 - n_dot_l, 5.0));
		let view_scatter = lerp(1.0, fd90, pow(1.0 - n_dot_v, 5.0));

		let f_diffuse = light_scatter * view_scatter * (1.0 / float.getPi()) * c_diff;
		let f_specular = F
				  * microfacet_dist(n_dot_h, alpha)
				  * mask_shadowing(n_dot_l, alpha)
				  / (4.0 * n_dot_v * n_dot_l + 0.0001);

		return (f_diffuse + f_specular) * n_dot_l * light.color;
	}
}


