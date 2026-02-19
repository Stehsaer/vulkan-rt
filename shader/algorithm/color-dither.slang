module color_dither;

namespace color_dither
{
	public func bayer_dither_2x2<int bits, int N>(input: vector<float, N>, pixel_pos: uint2)->vector<float, N>
	{
		let bayer_mat_2x2 = float2x2(0.25, 0.75, 1.0, 0.5);

		let pos = pixel_pos % uint2(2, 2);
		let threshold = bayer_mat_2x2[pos.y][pos.x];
		let levels = (1 << bits) - 1;

		let linear_quant = input * vector<float, N>(levels);
		let fractional = fract(linear_quant);
		let quantized = floor(linear_quant);
		let dithered = quantized + step(vector<float, N>(threshold), fractional);

		return dithered / vector<float, N>(levels);
	}

	public func bayer_dither_4x4<int bits, int N>(input: vector<float, N>, pixel_pos: uint2)->vector<float, N>
	{
		let bayer_mat_4x4 = (float4x4(
			0.0, 12.0, 3.0, 15.0,
			8.0, 4.0, 11.0, 7.0,
			2.0, 14.0, 1.0, 13.0,
			10.0, 6.0, 9.0, 5.0,
		) + 1.0) / 16.0;

		let pos = pixel_pos % uint2(4, 4);
		let threshold = bayer_mat_4x4[pos.y][pos.x];
		let levels = (1 << bits) - 1;

		let linear_quant = input * vector<float, N>(levels);
		let fractional = fract(linear_quant);
		let quantized = floor(linear_quant);
		let dithered = quantized + step(vector<float, N>(threshold), fractional);

		return dithered / vector<float, N>(levels);
	}
}


