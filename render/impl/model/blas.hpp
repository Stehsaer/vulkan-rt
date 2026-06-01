#pragma once

#include "common/util/error.hpp"
#include "render/model/blas.hpp"
#include "render/model/material.hpp"
#include "render/model/mesh.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::impl
{
	///
	/// @brief BLAS prototype for a mesh
	///
	///
	struct MeshBlasPrototype
	{
		vulkan::Buffer buffer;
		vk::raii::AccelerationStructureKHR blas;
		vk::DeviceSize scratch_size;

		/* Per-geometry info */

		std::vector<vk::AccelerationStructureGeometryKHR> geometries;
		std::vector<vk::AccelerationStructureBuildRangeInfoKHR> build_range;

		///
		/// @brief Create BLAS prototype for a mesh
		///
		/// @param context Vulkan context
		/// @param material_list Material list
		/// @param primitive_attrs Primitive attributes in the entire mesh-list
		/// @param vertex_buffer_addr Base address of the vertex buffer
		/// @param index_buffer_addr Base address of the index buffer
		/// @param mesh Mesh primitive index range
		/// @return Mesh BLAS prototype
		///
		[[nodiscard]]
		static std::expected<MeshBlasPrototype, Error> create(
			const vulkan::Context& context,
			const MaterialList& material_list,
			std::span<const PrimitiveAttribute> primitive_attrs,
			vk::DeviceAddress vertex_buffer_addr,
			vk::DeviceAddress index_buffer_addr,
			PrimitiveIndexRange mesh
		) noexcept;
	};

	///
	/// @brief Create BLAS prototypes for all primitives in the mesh list
	///
	/// @param context Vulkan context
	/// @param mesh_list Mesh list
	/// @param material_list Material list
	/// @return Array of prototypes, where indices correspond to the attribute array in mesh list
	///
	[[nodiscard]]
	std::expected<std::vector<MeshBlasPrototype>, Error> create_blas(
		const vulkan::Context& context,
		const MeshList& mesh_list,
		const MaterialList& material_list
	) noexcept;

	///
	/// @brief Result of building blas.
	/// @note See `BlasList` for details
	///
	struct BuildBlasResult
	{
		std::vector<BlasList::AccelStruct> blas_list;
	};

	///
	/// @brief Build BLASes
	///
	/// @param context Vulkan context
	/// @param prototypes Prototypes to be built
	/// @return Array of built BLASes
	///
	[[nodiscard]]
	std::expected<BuildBlasResult, Error> build_blas(
		const vulkan::Context& context,
		std::vector<MeshBlasPrototype> prototypes
	) noexcept;
}
