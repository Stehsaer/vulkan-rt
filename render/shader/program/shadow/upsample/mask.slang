/*===== IMPORT =====*/

import algorithm.coord;
import algorithm.id_swizzle;
import algorithm.octahedral;
import algorithm.reconstruct;
import internal.shadow;
import sv.compute;
import interop.camera;

/*===== STRUCT =====*/

struct Extent
{
	uint32_t2 half;
	uint32_t2 full;
};

/*===== I/O =====*/

Sampler2D<float> full_depth_tex;
Sampler2D<float2> full_normal_tex;  // Geometry normal
ConstantBuffer<Camera> camera;

// | MSB | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  | LSB |
// |     | NW | W  | SW | S  | SE | E  | NE | N  |     |
RWTexture2D<uint8_t> visibility_mask;

[[vk::push_constant]]
Extent extent;

/*===== CONSTANT =====*/

static const let NORMAL_THRES = 0.98;  // Approx. 78 deg
static const let DEPTH_THRES = 0.1;    // Approx. 6 deg

/*===== PROGRAM =====*/
// Generates bit-mask for faster plane detection

func task(full_pixcoord: uint2)->uint8_t
{
	let pixcenter = full_pixcoord + 0.5;
	let ref_z_depth = full_depth_tex.SampleLevelZero(pixcenter);
	let ref_encoded_normal = full_normal_tex.SampleLevelZero(pixcenter);

	let ndc_unit = float2(2.0, -2.0) / extent.full;
	let ref_ndc = texcoord_to_ndc(pixcenter / float2(extent.full));

	if (ref_z_depth == 0.0) return 0x00;

	let ref_view_pos = reconstruct::get_view_pos(camera.inv_proj, ref_ndc, ref_z_depth);
	let ref_world_normal = oct_decode(ref_encoded_normal);
	let ref_view_normal = mul(camera.view, float4(ref_world_normal, 0.0)).xyz;

	var mask : uint = 0;

	for (int i = 0; i < 8; i++)
	{
		let offset = NEIGHBOR_8_OFFSETS[i];

		let target_z_depth = full_depth_tex.SampleLevelZero(pixcenter + offset);
		let target_encoded_normal = full_normal_tex.SampleLevelZero(pixcenter + offset);

		let target_ndc = ref_ndc + ndc_unit * offset;
		let target_view_pos = reconstruct::get_view_pos(camera.inv_proj, target_ndc, target_z_depth);
		let target_world_normal = oct_decode(target_encoded_normal);

		let view_pos_diff = normalize(target_view_pos - ref_view_pos);
		let depth_angle_cos = abs(dot(view_pos_diff, ref_view_normal));
		let normal_angle_cos = dot(ref_world_normal, target_world_normal);

		let valid = depth_angle_cos < DEPTH_THRES && normal_angle_cos > NORMAL_THRES;

		mask |= (valid ? 1 : 0) << i;
	}

	return uint8_t(mask & 0xff);
}

[[shader("compute"), numthreads(16, 16, 1)]]
func main(sv: compute::ShaderVar)
{
	let half_pixcoord = swizzle_t4_b16(sv.global_thread_coord.xy);
	if (any(half_pixcoord >= extent.half)) return;
	visibility_mask.Store(half_pixcoord, task(half_pixcoord * 2));
}


