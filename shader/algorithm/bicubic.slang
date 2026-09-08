module bicubic;

namespace bicubic
{
	func cubic_hermite(x: float4, t: float)->float
	{
		let t2 = t * t;
		let t3 = t * t * t;
		let a = dot(x, float4(-0.5, 1.5, -1.5, 0.5));
		let b = dot(x, float4(1.0, -2.5, 2.0, -0.5));
		let c = -x.x / 2.0 + x.z / 2.0;
		let d = x.y;

		return a * t3 + b * t2 + c * t + d;
	}

	func sample_channel_red<let N : int>(
		img: Sampler2D<vector<float, N>>,
		texcoord: float2,
		fract_texcoord: float2
	)
		->float
	{
		let row1 = img.GatherRed(texcoord, int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1));
		let row1_cubic = cubic_hermite(row1, fract_texcoord.x);

		let row2 = img.GatherRed(texcoord, int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0));
		let row2_cubic = cubic_hermite(row2, fract_texcoord.x);

		let row3 = img.GatherRed(texcoord, int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1));
		let row3_cubic = cubic_hermite(row3, fract_texcoord.x);

		let row4 = img.GatherRed(texcoord, int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2));
		let row4_cubic = cubic_hermite(row4, fract_texcoord.x);

		return cubic_hermite(float4(row1_cubic, row2_cubic, row3_cubic, row4_cubic), fract_texcoord.y);
	}

	func sample_channel_green<let N : int>(
		img: Sampler2D<vector<float, N>>,
		texcoord: float2,
		fract_texcoord: float2
	)
		->float
	{
		let row1 = img.GatherGreen(texcoord, int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1));
		let row1_cubic = cubic_hermite(row1, fract_texcoord.x);

		let row2 = img.GatherGreen(texcoord, int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0));
		let row2_cubic = cubic_hermite(row2, fract_texcoord.x);

		let row3 = img.GatherGreen(texcoord, int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1));
		let row3_cubic = cubic_hermite(row3, fract_texcoord.x);

		let row4 = img.GatherGreen(texcoord, int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2));
		let row4_cubic = cubic_hermite(row4, fract_texcoord.x);

		return cubic_hermite(float4(row1_cubic, row2_cubic, row3_cubic, row4_cubic), fract_texcoord.y);
	}

	func sample_channel_blue<let N : int>(
		img: Sampler2D<vector<float, N>>,
		texcoord: float2,
		fract_texcoord: float2
	)
		->float
	{
		let row1 = img.GatherBlue(texcoord, int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1));
		let row1_cubic = cubic_hermite(row1, fract_texcoord.x);

		let row2 = img.GatherBlue(texcoord, int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0));
		let row2_cubic = cubic_hermite(row2, fract_texcoord.x);

		let row3 = img.GatherBlue(texcoord, int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1));
		let row3_cubic = cubic_hermite(row3, fract_texcoord.x);

		let row4 = img.GatherBlue(texcoord, int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2));
		let row4_cubic = cubic_hermite(row4, fract_texcoord.x);

		return cubic_hermite(float4(row1_cubic, row2_cubic, row3_cubic, row4_cubic), fract_texcoord.y);
	}

	func sample_channel_alpha<let N : int>(
		img: Sampler2D<vector<float, N>>,
		texcoord: float2,
		fract_texcoord: float2
	)
		->float
	{
		let row1 = img.GatherAlpha(texcoord, int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1));
		let row1_cubic = cubic_hermite(row1, fract_texcoord.x);

		let row2 = img.GatherAlpha(texcoord, int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0));
		let row2_cubic = cubic_hermite(row2, fract_texcoord.x);

		let row3 = img.GatherAlpha(texcoord, int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1));
		let row3_cubic = cubic_hermite(row3, fract_texcoord.x);

		let row4 = img.GatherAlpha(texcoord, int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2));
		let row4_cubic = cubic_hermite(row4, fract_texcoord.x);

		return cubic_hermite(float4(row1_cubic, row2_cubic, row3_cubic, row4_cubic), fract_texcoord.y);
	}

	func sample_channel(img: Sampler2D<float>, texcoord: float2, fract_texcoord: float2)->float
	{
		let row1 = img.GatherRed(texcoord, int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1));
		let row1_cubic = cubic_hermite(row1, fract_texcoord.x);

		let row2 = img.GatherRed(texcoord, int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0));
		let row2_cubic = cubic_hermite(row2, fract_texcoord.x);

		let row3 = img.GatherRed(texcoord, int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1));
		let row3_cubic = cubic_hermite(row3, fract_texcoord.x);

		let row4 = img.GatherRed(texcoord, int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2));
		let row4_cubic = cubic_hermite(row4, fract_texcoord.x);

		return cubic_hermite(float4(row1_cubic, row2_cubic, row3_cubic, row4_cubic), fract_texcoord.y);
	}

	// Bicubic-sample a 1-Channel image
	// - `img`: Image
	// - `coord`: Unnormalized coordinate
	// - **Return**: Sampled texel
	public func sample(img: Sampler2D<float>, coord: float2)->float
	{
		let texcoord = coord + 0.5;
		let fract_coord = fract(texcoord);
		let sample_texcoord = texcoord - fract_coord;

		return sample_channel(img, sample_texcoord, fract_coord);
	}

	// Bicubic-sample a 2-Channel image
	// - `img`: Image
	// - `coord`: Unnormalized coordinate
	// - **Return**: Sampled texel
	public func sample(img: Sampler2D<float2>, coord: float2)->float2
	{
		let texcoord = coord + 0.5;
		let fract_coord = fract(texcoord);
		let sample_texcoord = texcoord - fract_coord;

		return float2(
			sample_channel_red(img, sample_texcoord, fract_coord),
			sample_channel_green(img, sample_texcoord, fract_coord)
		);
	}

	// Bicubic-sample a 3-Channel image
	// - `img`: Image
	// - `coord`: Unnormalized coordinate
	// - **Return**: Sampled texel
	public func sample(img: Sampler2D<float3>, coord: float2)->float3
	{
		let texcoord = coord + 0.5;
		let fract_coord = fract(texcoord);
		let sample_texcoord = texcoord - fract_coord;

		return float3(
			sample_channel_red(img, sample_texcoord, fract_coord),
			sample_channel_green(img, sample_texcoord, fract_coord),
			sample_channel_blue(img, sample_texcoord, fract_coord)
		);
	}

	// Bicubic-sample a 4-Channel image
	// - `img`: Image
	// - `coord`: Unnormalized coordinate
	// - **Return**: Sampled texel
	public func sample(img: Sampler2D<float4>, coord: float2)->float4
	{
		let texcoord = coord + 0.5;
		let fract_coord = fract(texcoord);
		let sample_texcoord = texcoord - fract_coord;

		return float4(
			sample_channel_red(img, sample_texcoord, fract_coord),
			sample_channel_green(img, sample_texcoord, fract_coord),
			sample_channel_blue(img, sample_texcoord, fract_coord),
			sample_channel_alpha(img, sample_texcoord, fract_coord)
		);
	}
}


