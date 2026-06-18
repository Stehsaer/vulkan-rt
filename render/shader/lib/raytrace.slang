module raytrace;

import model;
import algorithm.coord;

namespace raytrace
{
	public struct FullHitInfo
	{
		public model::Vertex geometry;  // Geometry in world coordinate
		public uint32_t material_index;
	};

	public struct TexcoordHitInfo
	{
		public float2 texcoord;
		public uint32_t material_index;
	};

	// Note: See `render/resource/raytrace.hpp` for C++ counterpart
	public struct MeshResource
	{
		StructuredBuffer<model::PrimitiveAttribute> primitive_attributes;
		StructuredBuffer<model::Vertex> vertex_buffer;
		StructuredBuffer<uint32_t> index_buffer;

		func get_primitive_attr()->model::PrimitiveAttribute
		{
			let primitive_base_idx = InstanceID();
			let geometry_idx = GeometryIndex();

			return primitive_attributes[primitive_base_idx + geometry_idx];
		}

		func get_barycentrics(intersect_attr: BuiltInTriangleIntersectionAttributes)->float3
		{
			return float3(
				1.0 - intersect_attr.barycentrics.x - intersect_attr.barycentrics.y,
				intersect_attr.barycentrics
			);
		}

		// Get full hit information, suitable for closest-hit
		public func get_full_hitinfo(intersect_attr: BuiltInTriangleIntersectionAttributes)->FullHitInfo
		{
			let primitive_attr = get_primitive_attr();
			let base_index = primitive_attr.index_offset + 3 * PrimitiveIndex();

			let index0 = index_buffer[base_index + 0];
			let index1 = index_buffer[base_index + 1];
			let index2 = index_buffer[base_index + 2];

			let vertex0 = vertex_buffer[primitive_attr.vertex_offset + index0];
			let vertex1 = vertex_buffer[primitive_attr.vertex_offset + index1];
			let vertex2 = vertex_buffer[primitive_attr.vertex_offset + index2];

			let barycoord = get_barycentrics(intersect_attr);

			let local_position = bary_mix(vertex0.position, vertex1.position, vertex2.position, barycoord);
			let local_normal = bary_mix(vertex0.normal, vertex1.normal, vertex2.normal, barycoord);
			let local_tangent = bary_mix(vertex0.tangent, vertex1.tangent, vertex2.tangent, barycoord);
			let texcoord = bary_mix(vertex0.texcoord, vertex1.texcoord, vertex2.texcoord, barycoord);

			let transform = ObjectToWorld();
			let world_position = mul(transform, float4(local_position, 1.0));
			let world_normal = mul(transform, float4(local_normal, 0.0));
			let world_tangent = float4(mul(transform, float4(local_tangent.xyz, 0.0)), local_tangent.w);

			let world_geometry = model::Vertex(world_position, texcoord, world_normal, world_tangent);

			return FullHitInfo(world_geometry, primitive_attr.material_index);
		}

		// Get texcoord only hit information, suitable for any-hit
		public func get_texcoord_hitinfo(intersect_attr: BuiltInTriangleIntersectionAttributes)
			->TexcoordHitInfo
		{
			let primitive_attr = get_primitive_attr();
			let base_index = primitive_attr.index_offset + 3 * PrimitiveIndex();

			let index0 = index_buffer[base_index + 0];
			let index1 = index_buffer[base_index + 1];
			let index2 = index_buffer[base_index + 2];

			let texcoord0 = vertex_buffer[primitive_attr.vertex_offset + index0].texcoord;
			let texcoord1 = vertex_buffer[primitive_attr.vertex_offset + index1].texcoord;
			let texcoord2 = vertex_buffer[primitive_attr.vertex_offset + index2].texcoord;

			let barycoord = get_barycentrics(intersect_attr);
			let texcoord = bary_mix(texcoord0, texcoord1, texcoord2, barycoord);

			return TexcoordHitInfo(texcoord, primitive_attr.material_index);
		}
	};
}


