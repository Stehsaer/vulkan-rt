#pragma once

#include "common/util/error.hpp"
#include "model/mesh.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/common-context.hpp"

#include <coro/task.hpp>
#include <glm/glm.hpp>

namespace render
{
	constexpr uint32_t DEFAULT_MATERIAL = 0xFFFFFFFF;

	///
	/// @brief Attributes for a primitive
	///
	///
	struct PrimitiveAttribute
	{
		uint32_t index_offset;    // Offset of the first index into the index buffer
		uint32_t vertex_offset;   // Offset of the first vertex into the vertex buffer
		uint32_t vertex_count;    // Number of vertices in the primitive
		uint32_t material_index;  // `0xFFFFFFFF` if no material
		glm::vec3 aabb_min;       // Minimum corner of AlignedBound
		glm::vec3 aabb_max;       // Maximum corner of AlignedBound
	};

	///
	/// @brief Range of indices for a mesh
	/// @details #### Example
	/// `offset = 10, count = 5` means that the mesh has 5 primitives, starting from `primitive[10]` to
	/// `primitive[14]`
	///
	struct PrimitiveIndexRange
	{
		uint32_t offset, count;
	};

	///
	/// @brief Stores GPU resources for meshes (and its subresources, e.g. primitives)
	/// @note Refer to [detailed documentation](../doc/mesh.md)
	///
	class MeshList
	{
	  public:

		///
		/// @brief References to buffers
		/// @note Beware of the lifetime
		///
		struct Ref
		{
			vulkan::ArrayBufferRef<model::FullVertex> vertex_buffer;
			vulkan::ArrayBufferRef<uint32_t> index_buffer;
			vulkan::ArrayBufferRef<PrimitiveAttribute> primitive_attr_buffer;

			std::span<const PrimitiveIndexRange> mesh_ranges_array;
			std::span<const PrimitiveAttribute> primitive_attr_array;

			[[nodiscard]]
			const Ref* operator->() const noexcept
			{
				return this;
			}
		};

		///
		/// @brief Create a mesh list
		///
		/// @param context Vulkan context
		/// @param mesh Host-side meshes
		/// @return Created mesh list or error
		///
		[[nodiscard]]
		static std::expected<MeshList, Error> create(
			const vulkan::DeviceContext& context,
			std::span<const model::Mesh> mesh
		) noexcept;

		Ref operator->() const noexcept { return get(); }

		///
		/// @brief Get the references to the GPU buffers and counts
		///
		/// @return References to the GPU buffers and counts
		///
		[[nodiscard]]
		Ref get() const noexcept
		{
			return Ref{
				.vertex_buffer = vertex_buffer,
				.index_buffer = index_buffer,
				.primitive_attr_buffer = primitive_attr_buffer,
				.mesh_ranges_array = mesh_primitive_index_ranges,
				.primitive_attr_array = primitive_attributes
			};
		}

	  private:

		vulkan::ArrayBuffer<model::FullVertex> vertex_buffer;
		vulkan::ArrayBuffer<uint32_t> index_buffer;
		vulkan::ArrayBuffer<PrimitiveAttribute> primitive_attr_buffer;

		std::vector<PrimitiveIndexRange> mesh_primitive_index_ranges;
		std::vector<PrimitiveAttribute> primitive_attributes;

		explicit MeshList(
			vulkan::ArrayBuffer<model::FullVertex> vertex_buffer,
			vulkan::ArrayBuffer<uint32_t> index_buffer,
			vulkan::ArrayBuffer<PrimitiveAttribute> primitive_attr_buffer,
			std::vector<PrimitiveIndexRange> mesh_primitive_index_ranges,
			std::vector<PrimitiveAttribute> primitive_attributes
		) :
			vertex_buffer(std::move(vertex_buffer)),
			index_buffer(std::move(index_buffer)),
			primitive_attr_buffer(std::move(primitive_attr_buffer)),
			mesh_primitive_index_ranges(std::move(mesh_primitive_index_ranges)),
			primitive_attributes(std::move(primitive_attributes))
		{}

	  public:

		MeshList(const MeshList&) = delete;
		MeshList(MeshList&&) = default;
		MeshList& operator=(const MeshList&) = delete;
		MeshList& operator=(MeshList&&) = default;
	};
}
