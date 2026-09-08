/*===== IMPORT =====*/

import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import sv.compute;

import interop.camera;

/*===== Struct =====*/

struct Resolution
{
	uint32_t2 half;
	uint32_t2 full;
};

/*===== I/O =====*/

Sampler2D<float> half_shadow;
RWTexture2D<float> full_shadow;

// | MSB | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  | LSB |
// |     | NW | W  | SW | S  | SE | E  | NE | N  |     |
Sampler2D<uint8_t> visibility_mask;

[[vk::push_constant]]
Resolution res;

/*===== PROGRAM ====*/
// Upsample half-res shadow

func pixel_right(shadow: float4, mask_in: uint8_t4)->float
{
	let mask = (mask_in.xy & uint8_t2(1 << 2, 1 << 6)) != 0;
	if (!any(mask)) return (shadow.x + shadow.y) / 2;

	let weight = float2(mask);
	let value = shadow.xy * weight;
	return (value.x + value.y) / (weight.x + weight.y);
}

func pixel_bottom(shadow: float4, mask_in: uint8_t4)->float
{
	let mask = (mask_in.xz & uint8_t2(1 << 4, 1 << 0)) != 0;
	if (!any(mask)) return (shadow.x + shadow.z) / 2;

	let weight = float2(mask);
	let value = shadow.xz * weight;
	return (value.x + value.y) / (weight.x + weight.y);
}

func pixel_bottom_right(shadow: float4, mask_in: uint8_t4)->float
{
	let mask = (mask_in & uint8_t4(1 << 3, 1 << 5, 1 << 1, 1 << 7)) != 0;
	if (!any(mask)) return (shadow.x + shadow.y + shadow.z + shadow.w) / 4;

	let weight = float4(mask);
	let value = shadow * weight;
	return (value.x + value.y + value.z + value.w) / (weight.x + weight.y + weight.z + weight.w);
}

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let half_pixcoord = swizzle_t4_b16(sv.global_thread_coord.xy);
	if (any(half_pixcoord >= res.half)) return;

	let shadow = half_shadow.Gather(half_pixcoord + 1.0, int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1));
	let mask = uint8_t4(
		visibility_mask.SampleLevelZero(half_pixcoord + 0.5 + int2(0, 0)),
		visibility_mask.SampleLevelZero(half_pixcoord + 0.5 + int2(1, 0)),
		visibility_mask.SampleLevelZero(half_pixcoord + 0.5 + int2(0, 1)),
		visibility_mask.SampleLevelZero(half_pixcoord + 0.5 + int2(1, 1))
	);

	let pixel_right_value = pixel_right(shadow, mask);
	let pixel_bottom_value = pixel_bottom(shadow, mask);
	let pixel_bottom_right_value = pixel_bottom_right(shadow, mask);

	let full_pixcoord = half_pixcoord * 2;

	full_shadow.Store(full_pixcoord, shadow.x);

	if (all(full_pixcoord + int2(1, 0) < res.full))
		full_shadow.Store(full_pixcoord + int2(1, 0), pixel_right_value);

	if (all(full_pixcoord + int2(0, 1) < res.full))
		full_shadow.Store(full_pixcoord + int2(0, 1), pixel_bottom_value);

	if (all(full_pixcoord + int2(1, 1) < res.full))
		full_shadow.Store(full_pixcoord + int2(1, 1), pixel_bottom_right_value);
}


