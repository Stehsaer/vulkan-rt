#include "model/blas.hpp"
#include "common/util/align.hpp"
#include "common/util/error.hpp"
#include "model/material.hpp"
#include "model/mesh.hpp"
#include "render/model/material.hpp"
#include "render/model/mesh.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/util/command-runner.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <libassert/assert.hpp>
#include <ranges>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::impl
{
	// Future TODO: Compact
	static constexpr vk::BuildAccelerationStructureFlagsKHR BLAS_BUILD_FLAGS =
		vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;

	std::expected<MeshBlasPrototype, Error> MeshBlasPrototype::create(
		const vulkan::Context& context,
		const MaterialList& material_list,
		std::span<const PrimitiveAttribute> primitive_attrs,
		vk::DeviceAddress vertex_buffer_addr,
		vk::DeviceAddress index_buffer_addr,
		PrimitiveIndexRange mesh
	) noexcept
	{
		ASSERT(mesh.count > 0);

		const auto primitives = primitive_attrs.subspan(mesh.offset, mesh.count);

		const auto get_geometry =
			[&material_list, vertex_buffer_addr, index_buffer_addr](const PrimitiveAttribute& attribute) {
				const auto vertex_addr =
					vertex_buffer_addr + sizeof(model::FullVertex) * attribute.vertex_offset;
				const auto index_addr = index_buffer_addr + sizeof(uint32_t) * attribute.index_offset;

				const auto triangle_geometry = vk::AccelerationStructureGeometryTrianglesDataKHR{
					.vertexFormat = vk::Format::eR32G32B32Sfloat,
					.vertexData = vertex_addr,
					.vertexStride = sizeof(model::FullVertex),
					.maxVertex = attribute.vertex_count - 1,
					.indexType = vk::IndexType::eUint32,
					.indexData = index_addr
				};

				vk::GeometryFlagsKHR geometry_flags = {};
				if (material_list.query_material_mode(attribute.material_index).alpha_mode
					== model::AlphaMode::Opaque)
				{
					geometry_flags |= vk::GeometryFlagBitsKHR::eOpaque;
				}

				const auto geometry_info = vk::AccelerationStructureGeometryKHR{
					.geometryType = vk::GeometryTypeKHR::eTriangles,
					.geometry = triangle_geometry,
					.flags = geometry_flags,
				};

				return geometry_info;
			};

		const auto get_build_range = [](const PrimitiveAttribute& attribute) {
			ASSUME(attribute.index_count % 3 == 0);
			return vk::AccelerationStructureBuildRangeInfoKHR{
				.primitiveCount = attribute.index_count / 3,
				.primitiveOffset = 0,
				.firstVertex = 0,
				.transformOffset = 0
			};
		};

		const auto get_primitive_count = [](const PrimitiveAttribute& attribute) {
			ASSUME(attribute.index_count % 3 == 0);
			return attribute.index_count / 3;
		};

		auto geometries = std::vector(std::from_range, primitives | std::views::transform(get_geometry));
		auto build_ranges = std::vector(std::from_range, primitives | std::views::transform(get_build_range));
		const auto primitive_counts =
			std::vector(std::from_range, primitives | std::views::transform(get_primitive_count));

		const auto build_info =
			vk::AccelerationStructureBuildGeometryInfoKHR()
				.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
				.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
				.setFlags(BLAS_BUILD_FLAGS)
				.setGeometries(geometries);

		const auto build_sizes = context.device.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice,
			build_info,
			primitive_counts
		);

		/* Create BLAS */

		const auto blas_buffer_create_info = vk::BufferCreateInfo{
			.size = build_sizes.accelerationStructureSize,
			.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
				| vk::BufferUsageFlagBits::eShaderDeviceAddress
		};
		auto blas_buffer_result =
			context.allocator.create_buffer(blas_buffer_create_info, vulkan::MemoryUsage::GpuOnly);
		if (!blas_buffer_result) return blas_buffer_result.error().forward("Create BLAS buffer failed");
		auto blas_buffer = std::move(*blas_buffer_result);

		const auto blas_create_info = vk::AccelerationStructureCreateInfoKHR{
			.buffer = blas_buffer,
			.offset = 0,
			.size = build_sizes.accelerationStructureSize,
			.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		};
		auto blas_result = context.device.createAccelerationStructureKHR(blas_create_info);
		if (!blas_result) return Error::from(blas_result).forward("Create BLAS failed");

		return MeshBlasPrototype{
			.buffer = std::move(blas_buffer),
			.blas = std::move(*blas_result),
			.scratch_size = build_sizes.buildScratchSize,
			.geometries = std::move(geometries),
			.build_range = std::move(build_ranges),
		};
	}

	std::expected<std::vector<MeshBlasPrototype>, Error> create_blas(
		const vulkan::Context& context,
		const MeshList& mesh_list,
		const MaterialList& material_list
	) noexcept
	{
		const vk::DeviceAddress vertex_buffer_addr =
			context.device.getBufferAddress({.buffer = mesh_list->vertex_buffer});
		const vk::DeviceAddress index_buffer_addr =
			context.device.getBufferAddress({.buffer = mesh_list->index_buffer});

		auto blas_prototypes_result =
			mesh_list->mesh_ranges_array
			| std::views::transform(
				[vertex_buffer_addr, index_buffer_addr, &context, &material_list, &mesh_list](
					const PrimitiveIndexRange& range
				) {
					return impl::MeshBlasPrototype::create(
						context,
						material_list,
						mesh_list->primitive_attr_array,
						vertex_buffer_addr,
						index_buffer_addr,
						range
					);
				}
			)
			| Error::collect();

		if (!blas_prototypes_result)
			return blas_prototypes_result.error().forward("Create BLAS prototypes failed");

		return blas_prototypes_result;
	}

	// Minimum scratch buffer size of 64MiB
	static constexpr auto MIN_SCRATCH_BUFFER_SIZE = 64 * 1048576zu;

	std::expected<BuildBlasResult, Error> build_blas(
		const vulkan::Context& context,
		std::vector<MeshBlasPrototype> prototypes
	) noexcept
	{
		/* Create Environment */

		// Get scratch alignment
		const auto as_properties =
			context.phy_device
				.getProperties2<
					vk::PhysicalDeviceProperties2,
					vk::PhysicalDeviceAccelerationStructurePropertiesKHR
				>()
				.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
		const auto scratch_alignment = as_properties.minAccelerationStructureScratchOffsetAlignment;

		// Find maximum needed scratch buffer size
		const auto max_scratch_size =
			std::ranges::max(prototypes | std::views::transform(&impl::MeshBlasPrototype::scratch_size));
		const auto max_aligned_scratch_size = util::align_address(max_scratch_size, scratch_alignment);
		const auto scratch_size = std::max(MIN_SCRATCH_BUFFER_SIZE, max_aligned_scratch_size);

		// Create scratch buffer
		auto scratch_buffer_result = context.allocator.create_buffer(
			{
				.size = scratch_size + scratch_alignment,
				.usage =
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			},
			vulkan::MemoryUsage::GpuOnly
		);
		if (!scratch_buffer_result)
			return scratch_buffer_result.error().forward("Create scratch buffer failed");
		auto scratch_buffer = std::move(*scratch_buffer_result);
		const auto scratch_addr = util::align_address(
			context.device.getBufferAddress({.buffer = scratch_buffer}),
			scratch_alignment
		);

		auto command_runner_result = vulkan::CommandRunner::create(context);
		if (!command_runner_result)
			return command_runner_result.error().forward("Create command runner failed");
		auto command_runner = std::move(*command_runner_result);

		/* Sort by scratch size */

		std::vector<size_t> prototype_index(std::from_range, std::views::iota(0zu, prototypes.size()));
		std::ranges::sort(prototype_index, [&prototypes](size_t left, size_t right) {
			return prototypes[left].scratch_size < prototypes[right].scratch_size;
		});

		while (!prototype_index.empty())
		{
			auto current_base = scratch_addr;

			std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> build_geometry_infos;
			std::vector<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>> build_range_infos;

			/* Add tasks */

			while (!prototype_index.empty())
			{
				const auto& prototype = prototypes[prototype_index.back()];
				const auto aligned_scratch_size =
					util::align_address(prototype.scratch_size, scratch_alignment);

				// Break the loop when exceeds scratch size
				if (current_base + aligned_scratch_size > scratch_size + scratch_addr) break;

				build_geometry_infos.emplace_back(
					vk::AccelerationStructureBuildGeometryInfoKHR()
						.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
						.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
						.setDstAccelerationStructure(prototype.blas)
						.setScratchData(current_base)
						.setFlags(BLAS_BUILD_FLAGS)
						.setGeometries(prototype.geometries)
				);
				build_range_infos.emplace_back(prototype.build_range);

				prototype_index.pop_back();
				current_base += aligned_scratch_size;
			}

			/* Build */

			const auto build_range_info_ptrs = build_range_infos
				| std::views::transform([](const auto& obj) { return obj.data(); })
				| std::ranges::to<std::vector>();

			const auto run_result = command_runner.run(
				context,
				[&build_range_info_ptrs,
				 &build_geometry_infos](const vk::raii::CommandBuffer& command_buffer) {
					command_buffer
						.buildAccelerationStructuresKHR(build_geometry_infos, build_range_info_ptrs);
				}
			);
			if (!run_result) return run_result.error().forward("Build acceleration structure failed");
		}

		auto blas_list =
			prototypes
			| std::views::as_rvalue
			| std::views::transform([](MeshBlasPrototype prototype) {
				  return std::make_pair(std::move(prototype.buffer), std::move(prototype.blas));
			  })
			| std::ranges::to<std::vector>();

		return BuildBlasResult{
			.blas_list = std::move(blas_list),
		};
	}
}
