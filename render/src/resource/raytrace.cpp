#include "render/resource/raytrace.hpp"
#include "common/util/error.hpp"
#include "render/model/mesh.hpp"
#include "render/model/model.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/pool-size.hpp"
#include "vulkan/util/descriptor-set-layout.hpp"

#include <expected>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	using MeshResourceLayout = vulkan::MonoDescriptorSetLayout<
		vulkan::MonoDescriptorSetSlot<
			vk::DescriptorType::eStorageBuffer,
			vk::ShaderStageFlagBits::eAnyHitKHR,
			vk::ShaderStageFlagBits::eClosestHitKHR
		>,
		vulkan::MonoDescriptorSetSlot<
			vk::DescriptorType::eStorageBuffer,
			vk::ShaderStageFlagBits::eAnyHitKHR,
			vk::ShaderStageFlagBits::eClosestHitKHR
		>,
		vulkan::MonoDescriptorSetSlot<
			vk::DescriptorType::eStorageBuffer,
			vk::ShaderStageFlagBits::eAnyHitKHR,
			vk::ShaderStageFlagBits::eClosestHitKHR
		>
	>;

	std::expected<RaytraceResourceLayout, Error> RaytraceResourceLayout::create(
		const vulkan::Context& context
	) noexcept
	{
		return MeshResourceLayout::create_descriptor_set_layout(context).transform(
			[](vk::raii::DescriptorSetLayout layout) { return RaytraceResourceLayout(std::move(layout)); }
		);
	}

	std::expected<RaytraceResource, Error> RaytraceResource::create(
		const vulkan::Context& context,
		const RaytraceResourceLayout& layout,
		const Model& model
	) noexcept
	{
		/*===== Create descriptor set =====*/

		static constexpr auto BINDINGS = MeshResourceLayout::get_bindings();
		const auto pool_sizes = vulkan::calc_pool_sizes(BINDINGS, 1);

		auto pool_result = context.device.createDescriptorPool(
			vk::DescriptorPoolCreateInfo()
				.setPoolSizes(pool_sizes)
				.setMaxSets(1)
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
		);
		if (!pool_result) return Error::from(pool_result);
		auto pool = std::move(*pool_result);

		const vk::DescriptorSetLayout raw_layout = layout->mesh_resource;
		auto set_result = context.device.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo().setSetLayouts(raw_layout).setDescriptorPool(pool)
		);
		if (!set_result) return Error::from(set_result);
		auto set = std::move(set_result->at(0));

		/*===== Write descriptor set =====*/

		const auto primitive_buffer_info = vk::DescriptorBufferInfo{
			.buffer = model.mesh_list->primitive_attr_buffer,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto vertex_buffer_info = vk::DescriptorBufferInfo{
			.buffer = model.mesh_list->vertex_buffer,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto index_buffer_info = vk::DescriptorBufferInfo{
			.buffer = model.mesh_list->index_buffer,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto write_sets = MeshResourceLayout::get_write_infos(
			set,
			primitive_buffer_info,
			vertex_buffer_info,
			index_buffer_info
		);
		context.device.updateDescriptorSets(write_sets, {});

		return RaytraceResource(std::move(pool), std::move(set));
	}
}
