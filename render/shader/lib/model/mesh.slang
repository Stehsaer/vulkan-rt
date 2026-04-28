implementing model;

namespace model
{
	// Primitive attributes
	public struct PrimitiveAttribute
	{
		public static const uint32_t DEFAULT_MATERIAL = 0xFFFFFFFF;

		public uint32_t index_offset;   // Offset of the first index into the index buffer
		public uint32_t vertex_offset;  // Offset of the first vertex into the vertex buffer
		public uint32_t vertex_count;   // Number of vertices in the primitive

		// Material index for the primitive, or `DEFAULT_MATERIAL` if default material
		public uint32_t material_index;

		public float3 aabb_min;  // Minimum corner of the AABB for the primitive
		public float3 aabb_max;  // Maximum corner of the AABB for the primitive
	};

	// Vertex format for the model
	public struct Vertex
	{
		[[vk::location(0)]]
		public float3 position;

		[[vk::location(1)]]
		public float2 texcoord;

		[[vk::location(2)]]
		public float3 normal;

		[[vk::location(3)]]
		public float4 tangent;
	};
}


