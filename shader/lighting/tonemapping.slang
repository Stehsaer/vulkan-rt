module tonemapping;

namespace tonemapping
{
	public func reinhard(color: float3)->float3
	{
		return color / (color + 1.0);
	}

	public func aces(color: float3)->float3
	{
		let a = 2.51;
		let b = 0.03;
		let c = 2.43;
		let d = 0.59;
		let e = 0.14;

		return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
	}

	namespace agx_impl
	{
		func to_linear(sRGB: float3)->float3
		{
			let cutoff = sRGB < float3(0.04045);
			let higher = pow((sRGB + float3(0.055)) / float3(1.055), float3(2.4));
			let lower = sRGB / float3(12.92);

			return lerp(higher, lower, float3(cutoff));
		}

		func from_linear(linearRGB: float3)->float3
		{
			let cutoff = linearRGB < float3(0.0031308);
			let higher = float3(1.055) * pow(linearRGB, float3(1.0 / 2.4)) - float3(0.055);
			let lower = linearRGB * float3(12.92);

			return lerp(higher, lower, float3(cutoff));
		}

		func agx_curve3(v: float3)->float3
		{
			let threshold = 0.6060606060606061;
			let a_up = 69.86278913545539;
			let a_down = 59.507875;
			let b_up = 13.0 / 4.0;
			let b_down = 3.0 / 1.0;
			let c_up = -4.0 / 13.0;
			let c_down = -1.0 / 3.0;

			let mask = step(v, float3(threshold));
			let a = a_up + (a_down - a_up) * mask;
			let b = b_up + (b_down - b_up) * mask;
			let c = c_up + (c_down - c_up) * mask;

			return 0.5 + (((-2.0 * threshold)) + 2.0 * v) * pow(1.0 + a * pow(abs(v - threshold), b), c);
		}
	}

	public func agx<bool output_linear>(linear_light: float3)->float3
	{
		let min_ev = -12.473931188332413;
		let max_ev = 4.026068811667588;
		let dynamic_range = max_ev - min_ev;

		let agx_mat = float3x3(
			0.8424010709504686,
			0.07843650156180276,
			0.0791624274877287,
			0.04240107095046854,
			0.8784365015618028,
			0.0791624274877287,
			0.04240107095046854,
			0.07843650156180276,
			0.8791624274877287
		);

		let agx_mat_inv = float3x3(
			1.1969986613119143,
			-0.09804562695225345,
			-0.09895303435966087,
			-0.053001338688085674,
			1.1519543730477466,
			-0.09895303435966087,
			-0.053001338688085674,
			-0.09804562695225345,
			1.151046965640339
		);

		// Input transform (inset)
		let ci = mul(agx_mat, linear_light);

		// Apply sigmoid function
		let ct = saturate(fma(log2(ci), float3(1.0 / dynamic_range), float3(-min_ev / dynamic_range)));
		let co = mul(agx_mat_inv, agx_impl::agx_curve3(ct));

		if (output_linear)
			return agx_impl::from_linear(co);
		else
			return co;
	}
}


