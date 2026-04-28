module indirect_drawcall;

import primitive_drawcall;
import model;

// Indirect drawcall
public struct IndirectDrawcall
{
	public uint32_t index_count;
	public uint32_t instance_count;
	public uint32_t first_index;
	public int32_t vertex_offset;
	public uint32_t first_instance;

	public PrimitiveDrawcall drawcall;

	public static func from(
		attr: model::PrimitiveAttribute,
		drawcall: PrimitiveDrawcall,
		visible: bool,
		first_instance: uint32_t
	)
		->IndirectDrawcall
	{
		var entry : IndirectDrawcall;
		entry.index_count = attr.vertex_count;
		entry.instance_count = select(visible, 1, 0);
		entry.first_index = attr.index_offset;
		entry.vertex_offset = attr.vertex_offset;
		entry.first_instance = first_instance;
		entry.drawcall = drawcall;
		return entry;
	}
};


