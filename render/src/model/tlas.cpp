#include "render/model/tlas.hpp"
#include "common/util/align.hpp"
#include "common/util/error.hpp"
#include "model/hierarchy.hpp"
#include "render/model/model.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/glm.hpp"
#include "vulkan/util/static-resource-creator.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/matrix_float4x4.hpp>
#include <limits>
#include <mutex>
#include <ranges>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace render
{
	namespace
	{
		std::expected<vulkan::ArrayBuffer<vk::AccelerationStructureInstanceKHR>, Error>
		create_instance_buffer(
			const vulkan::Context& context,
			const Model& model,
			std::span<const glm::mat4> transforms
		) noexcept
		{
			const auto get_tlas_instance =
				[&model, &context, &transforms](const model::Hierarchy::Drawcall& drawcall) {
					const auto blas_address = context.device.getAccelerationStructureAddressKHR(
						{.accelerationStructure = model.blas_list->blas[drawcall.mesh_index]}
					);

					return vk::AccelerationStructureInstanceKHR{
						.transform = vulkan::to<vk::TransformMatrixKHR>(transforms[drawcall.node_index]),
						.instanceCustomIndex = model.mesh_list->mesh_ranges_array[drawcall.mesh_index].offset,
						.mask = 0,
						.instanceShaderBindingTableRecordOffset = 0,
						.flags = {},
						.accelerationStructureReference = blas_address
					};
				};

			auto instances = std::vector(
				std::from_range,
				model.hierarchy.get_renderables() | std::views::transform(get_tlas_instance)
			);

			vulkan::StaticResourceCreator resource_creator(context);

			auto instance_buffer_result = resource_creator.create_array_buffer(
				instances,
				vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
					| vk::BufferUsageFlagBits::eShaderDeviceAddress
			);
			if (!instance_buffer_result)
				return instance_buffer_result.error().forward("Create instance buffer failed");
			auto instance_buffer = std::move(*instance_buffer_result);

			if (const auto result = resource_creator.execute_uploads(); !result)
				return result.error().forward("Upload resources failed");

			return instance_buffer;
		}
	}

	std::expected<Tlas, Error> Tlas::build(
		const vulkan::Context& context,
		const Model& model,
		std::span<const glm::mat4> transforms
	) noexcept
	{
		/* Get TLAS instances */

		auto instance_buffer_result = create_instance_buffer(context, model, transforms);
		if (!instance_buffer_result) return instance_buffer_result.error();
		auto instance_buffer = std::move(*instance_buffer_result);

		/* Create TLAS */

		const auto as_properties =
			context.phy_device
				.getProperties2<
					vk::PhysicalDeviceProperties2,
					vk::PhysicalDeviceAccelerationStructurePropertiesKHR
				>()
				.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
		const auto scratch_alignment = as_properties.minAccelerationStructureScratchOffsetAlignment;

		const auto geometry =
			vk::AccelerationStructureGeometryKHR()
				.setGeometryType(vk::GeometryTypeKHR::eInstances)
				.setGeometry(
					vk::AccelerationStructureGeometryInstancesDataKHR{
						.data = context.device.getBufferAddress({.buffer = instance_buffer})
					}
				);

		auto build_info =
			vk::AccelerationStructureBuildGeometryInfoKHR()
				.setType(vk::AccelerationStructureTypeKHR::eTopLevel)
				.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
				.setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
				.setGeometries(geometry);  // NOTE: incomplete at this moment

		const auto build_range_info = vk::AccelerationStructureBuildRangeInfoKHR{
			.primitiveCount = static_cast<uint32_t>(model.hierarchy.get_renderables().size()),
			.primitiveOffset = 0,
			.firstVertex = 0,
			.transformOffset = 0
		};

		const auto build_sizes = context.device.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice,
			build_info,
			{static_cast<uint32_t>(model.hierarchy.get_renderables().size())}
		);

		// Create scratch & as-storage buffer
		auto scratch_buffer_result = context.allocator.create_buffer(
			{
				.size = build_sizes.buildScratchSize + scratch_alignment,
				.usage =
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			},
			vulkan::MemoryUsage::GpuOnly
		);
		auto tlas_buffer_result = context.allocator.create_buffer(
			{
				.size = build_sizes.accelerationStructureSize,
				.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
					| vk::BufferUsageFlagBits::eShaderDeviceAddress,
			},
			vulkan::MemoryUsage::GpuOnly
		);

		if (!scratch_buffer_result)
			return scratch_buffer_result.error().forward("Create scratch buffer failed");
		if (!tlas_buffer_result) return tlas_buffer_result.error().forward("Create TLAS buffer failed");

		auto scratch_buffer = std::move(*scratch_buffer_result);
		auto tlas_buffer = std::move(*tlas_buffer_result);

		build_info.setScratchData(
			util::align_address(
				context.device.getBufferAddress({.buffer = scratch_buffer}),
				scratch_alignment
			)
		);

		// Create TLAS
		const auto tlas_create_info = vk::AccelerationStructureCreateInfoKHR{
			.buffer = tlas_buffer,
			.offset = 0,
			.size = build_sizes.accelerationStructureSize,
			.type = vk::AccelerationStructureTypeKHR::eTopLevel,
		};
		auto tlas_result = context.device.createAccelerationStructureKHR(tlas_create_info);
		if (!tlas_result) return Error::from(tlas_result).forward("Create BLAS failed");
		auto tlas = std::move(*tlas_result);

		build_info.setDstAccelerationStructure(tlas);

		/* Build TLAS */

		// Create command pool
		auto command_pool_result = context.device.createCommandPool({
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = context.family,
		});
		if (!command_pool_result) return Error::from(command_pool_result);
		auto command_pool = std::move(*command_pool_result);

		// Create command buffer
		auto command_buffer_result = context.device.allocateCommandBuffers({
			.commandPool = command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		});
		if (!command_buffer_result) return Error::from(command_buffer_result);
		auto command_buffer = std::move(command_buffer_result->at(0));

		// Create fence
		auto fence_result = context.device.createFence({});
		if (!fence_result) return Error::from(fence_result);
		auto fence = std::move(*fence_result);

		if (const auto result = command_buffer.begin({}); !result) return Error::from(result);

		command_buffer.buildAccelerationStructuresKHR({build_info}, {&build_range_info});

		if (const auto result = command_buffer.end(); !result) return Error::from(result);

		const auto command_buffer_info = vk::CommandBufferSubmitInfo{.commandBuffer = command_buffer};
		const auto submit_info = vk::SubmitInfo2().setCommandBufferInfos(command_buffer_info);

		{
			const std::scoped_lock lock(context.submit_mutex);

			if (const auto submit_result = context.queue.submit2(submit_info, fence); !submit_result)
				return Error::from(submit_result);
		}

		if (const auto result =
				context.device.waitForFences({fence}, vk::True, std::numeric_limits<uint64_t>::max());
			result != vk::Result::eSuccess)
			return Error::from(result);

		return Tlas(std::move(instance_buffer), std::move(tlas_buffer), std::move(tlas));
	}
}
