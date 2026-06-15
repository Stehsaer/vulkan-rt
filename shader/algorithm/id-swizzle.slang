module id_swizzle;

namespace id_swizzle
{
	public func swizzle<uint tile_size, uint group_size>(invocation_id: uint)->uint2
	{
		constexpr let tiles_per_row = group_size / tile_size;

		let tile_id = invocation_id / 16;
		let local_id = invocation_id % 16;

		let tile_pos = uint2(tile_id % tiles_per_row, tile_id / tiles_per_row) * uint2(tile_size, tile_size);
		let local_pos = uint2(local_id % tile_size, local_id / tile_size);

		return tile_pos + local_pos;
	}

}

// Thread-ID swizzle, for block-size 16 and tile-size 4
// - `coord`: Thread coordinate. Works for both local and global coordinate
public func swizzle_t4_b16(coord: uint2)->uint2
{
	return uint2(
		(coord.x & 0xfffffff0) | ((coord.y & 0x3) << 2) | (coord.x & 0x3),
		(coord.y & 0xfffffff0) | (coord.y & 0xc) | ((coord.x & 0xc) >> 2)
	);
}


