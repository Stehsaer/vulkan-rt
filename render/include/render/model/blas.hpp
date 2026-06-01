#pragma once

#include "common/util/error.hpp"
#include "render/model/material.hpp"
#include "render/model/mesh.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief List of BLASes
	/// @details
	/// #### Indexing Scheme
	/// - Each BLAS in `blas_list` corresponds to the mesh at the same index as `MeshList->mesh_ranges_array`,
	/// where different primitives in that mesh are grouped as different sub-geometries in the BLAS
	/// - Each geometry in `blas_list` corresponds to the primitive at the same index inside the mesh's
	/// primitive range
	///
	/// #### Implementation Hint
	/// Set `instanceCustomIndex` to `mesh.offset` when creating TLAS, where `mesh` is the corresponding
	/// mesh acquired from `MeshList`.
	///
	class BlasList
	{
	  public:

		using AccelStruct = std::pair<vulkan::Buffer, vk::raii::AccelerationStructureKHR>;
		using AccelStructView = std::ranges::transform_view<
			std::ranges::ref_view<const std::vector<AccelStruct>>,
			vk::raii::AccelerationStructureKHR AccelStruct::*
		>;

		///
		/// @brief Create and build BLASes
		///
		/// @param context Vulkan context
		/// @param mesh_list Mesh list
		/// @param material_list Material list
		/// @return Created and built BLASes or error
		///
		[[nodiscard]]
		static std::expected<BlasList, Error> create(
			const vulkan::Context& context,
			const MeshList& mesh_list,
			const MaterialList& material_list
		) noexcept;

		struct ReadonlyWrapper
		{
			///
			/// @brief BLASes for each mesh
			///
			AccelStructView blas;

			const ReadonlyWrapper* operator->() const noexcept { return this; }
		};

		ReadonlyWrapper operator->() const noexcept
		{
			return ReadonlyWrapper{
				.blas = blas_list | std::views::transform(&AccelStruct::second),
			};
		}

	  private:

		std::vector<AccelStruct> blas_list;

		explicit BlasList(std::vector<AccelStruct> blas_list) :
			blas_list(std::move(blas_list))
		{}

	  public:

		BlasList(const BlasList&) = delete;
		BlasList(BlasList&&) = default;
		BlasList& operator=(const BlasList&) = delete;
		BlasList& operator=(BlasList&&) = default;
	};
}
