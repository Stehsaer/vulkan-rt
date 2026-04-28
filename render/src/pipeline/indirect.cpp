#include "render/pipeline/indirect.hpp"
#include "common/util/array.hpp"
#include "render/util/per-render-state.hpp"
#include "shader/indirect.hpp"
#include "vulkan/util/pool-size.hpp"
#include "vulkan/util/shader.hpp"

#include <ranges>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace render
{
	static consteval auto get_descriptor_set_bindings() noexcept
	{
		constexpr auto indirect_entries_binding = vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
		};

		constexpr auto drawcalls_binding = vk::DescriptorSetLayoutBinding{
			.binding = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
		};

		constexpr auto node_transforms_binding = vk::DescriptorSetLayoutBinding{
			.binding = 2,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
		};

		constexpr auto primitive_attrs_binding = vk::DescriptorSetLayoutBinding{
			.binding = 3,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
		};

		return std::to_array({
			indirect_entries_binding,
			drawcalls_binding,
			node_transforms_binding,
			primitive_attrs_binding,
		});
	}

	static std::expected<vk::raii::DescriptorSetLayout, Error> create_descriptor_set_layout(
		const vulkan::DeviceContext& context
	) noexcept
	{
		constexpr auto bindings = get_descriptor_set_bindings();

		auto layout_result =
			context.device
				.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo().setBindings(bindings))
				.transform_error(Error::from<vk::Result>());
		if (!layout_result) return layout_result.error().forward("Create descriptor set layout failed");
		return std::move(*layout_result);
	}

	static std::expected<vk::raii::PipelineLayout, Error> create_pipeline_layout(
		const vulkan::DeviceContext& context,
		const vk::raii::DescriptorSetLayout& data_descriptor_set_layout
	) noexcept
	{
		const auto set_layouts = std::to_array<const vk::DescriptorSetLayout>({data_descriptor_set_layout});

		const auto push_constant_range = vk::PushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
			.offset = 0,
			.size = sizeof(uint32_t)
		};

		const auto create_info =
			vk::PipelineLayoutCreateInfo()
				.setSetLayouts(set_layouts)
				.setPushConstantRanges(push_constant_range);

		auto layout_result =
			context.device.createPipelineLayout(create_info).transform_error(Error::from<vk::Result>());
		if (!layout_result) return layout_result.error().forward("Create pipeline layout failed");
		return std::move(*layout_result);
	}

	std::expected<IndirectPipeline, Error> IndirectPipeline::create(
		const vulkan::DeviceContext& context
	) noexcept
	{
		auto shader_module_result = vulkan::create_shader(context.device, shader::indirect);
		if (!shader_module_result) return shader_module_result.error().forward("Create shader module failed");
		auto shader_module = std::move(*shader_module_result);

		auto descriptor_set_layout_result = create_descriptor_set_layout(context);
		if (!descriptor_set_layout_result)
			return descriptor_set_layout_result.error().forward("Create descriptor set layout failed");
		auto data_descriptor_set_layout = std::move(*descriptor_set_layout_result);

		auto pipeline_layout_result = create_pipeline_layout(context, data_descriptor_set_layout);
		if (!pipeline_layout_result)
			return pipeline_layout_result.error().forward("Create pipeline layout failed");
		auto pipeline_layout = std::move(*pipeline_layout_result);

		const auto pipeline_stage_create_info =
			vk::PipelineShaderStageCreateInfo()
				.setStage(vk::ShaderStageFlagBits::eCompute)
				.setModule(shader_module)
				.setPName("main");
		const auto pipeline_create_info =
			vk::ComputePipelineCreateInfo().setStage(pipeline_stage_create_info).setLayout(pipeline_layout);

		auto pipeline_result =
			context.device.createComputePipeline(nullptr, pipeline_create_info)
				.transform_error(Error::from<vk::Result>());
		if (!pipeline_result) return pipeline_result.error().forward("Create compute pipeline failed");
		auto pipeline = std::move(*pipeline_result);

		return IndirectPipeline(
			std::move(data_descriptor_set_layout),
			std::move(pipeline_layout),
			std::move(pipeline)
		);
	}

	std::expected<std::vector<IndirectPipeline::ResourceSet>, Error> IndirectPipeline::create_resource_sets(
		const vulkan::DeviceContext& context,
		uint32_t count
	) const noexcept
	{
		/*
		 * NOTE: Each resource set contains 4 descriptor sets for each material variant
		 */

		static constexpr auto SETS_PER_RESOURCE_SET = 4;

		const auto descriptor_bindings = get_descriptor_set_bindings();
		const auto descriptor_pool_sizes =
			vulkan::calc_pool_sizes(descriptor_bindings, count * SETS_PER_RESOURCE_SET);

		auto descriptor_pool_result =
			context.device
				.createDescriptorPool(
					vk::DescriptorPoolCreateInfo()
						.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
						.setMaxSets(count * SETS_PER_RESOURCE_SET)
						.setPoolSizes(descriptor_pool_sizes)
				)
				.transform_error(Error::from<vk::Result>());
		if (!descriptor_pool_result)
			return descriptor_pool_result.error().forward("Create descriptor pool failed");
		auto descriptor_pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*descriptor_pool_result));

		const auto layouts = std::vector(SETS_PER_RESOURCE_SET, *descriptor_set_layout);
		const auto set_alloc_info =
			vk::DescriptorSetAllocateInfo().setDescriptorPool(*descriptor_pool).setSetLayouts(layouts);

		const auto create_resource_set_fn =
			[&set_alloc_info, &context, &descriptor_pool] -> std::expected<ResourceSet, Error> {
			auto sets_result =
				context.device.allocateDescriptorSets(set_alloc_info)
					.transform_error(Error::from<vk::Result>());
			if (!sets_result) return sets_result.error().forward("Allocate descriptor sets failed");
			auto sets = std::move(*sets_result);

			return ResourceSet(
				descriptor_pool,
				{
					.opaque_single_sided = std::move(sets[0]),
					.opaque_double_sided = std::move(sets[1]),
					.masked_single_sided = std::move(sets[2]),
					.masked_double_sided = std::move(sets[3]),
				}
			);
		};

		return std::views::repeat(create_resource_set_fn, count)
			| std::views::transform([](auto&& f) { return f(); })
			| Error::collect();
	}

	void IndirectPipeline::compute(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);

		for (const auto [descriptor_set, buffer] :
			 std::views::zip(resource_set.descriptor_sets.all(), resource_set.indirect_buffers.all()))
		{
			if (buffer.count() == 0) continue;

			const auto group_count = (buffer.count() + WORKGROUP_SIZE) / WORKGROUP_SIZE;
			const auto push_constant = static_cast<uint32_t>(buffer.count());

			command_buffer
				.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, *descriptor_set, {});

			command_buffer.pushConstants<uint32_t>(
				*pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				push_constant
			);

			command_buffer.dispatch(group_count, 1, 1);
		}

		static constexpr auto get_sync_barrier = [](vk::Buffer buffer) {
			return vk::BufferMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.dstStageMask =
					vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eVertexShader,
				.dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eShaderRead,
				.buffer = buffer,
				.offset = 0,
				.size = vk::WholeSize
			};
		};

		const auto barriers = resource_set.indirect_buffers.as_ref_array()
			| util::map_array([](auto&& buffer) { return get_sync_barrier(buffer.get()); });

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setBufferMemoryBarriers(barriers));
	}

	void IndirectPipeline::ResourceSet::update(
		const vulkan::DeviceContext& context,
		const Model& model,
		const HostDrawcallResource& drawcall_resource,
		const IndirectResource& indirect_resource
	) noexcept
	{
		for (const auto& [descriptor_set, self_indirect_buffer, indirect_buffer, drawcall_buffer] :
			 std::views::zip(
				 descriptor_sets.all(),
				 this->indirect_buffers.all(),
				 indirect_resource.ref().all(),
				 drawcall_resource->primitive_drawcalls.all()
			 ))
		{
			const auto indirect_write_buffer_info = vk::DescriptorBufferInfo{
				.buffer = indirect_buffer,
				.offset = 0,
				.range = indirect_buffer.size_vk()
			};

			const auto primitive_drawcall_buffer_info = vk::DescriptorBufferInfo{
				.buffer = drawcall_buffer,
				.offset = 0,
				.range = drawcall_buffer.size_vk()
			};

			const auto transform_buffer_info = vk::DescriptorBufferInfo{
				.buffer = drawcall_resource->transform,
				.offset = 0,
				.range = drawcall_resource->transform.size_vk()
			};

			const auto primitive_attr_buffer_info = vk::DescriptorBufferInfo{
				.buffer = model.mesh_list->primitive_attr_buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			const auto indirect_write_descriptor_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &indirect_write_buffer_info
			};

			const auto primitive_drawcall_descriptor_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &primitive_drawcall_buffer_info
			};

			const auto transform_descriptor_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &transform_buffer_info
			};

			const auto primitive_attr_descriptor_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 3,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &primitive_attr_buffer_info
			};

			const auto write_descriptor_sets = std::to_array({
				indirect_write_descriptor_set,
				primitive_drawcall_descriptor_set,
				transform_descriptor_set,
				primitive_attr_descriptor_set,
			});

			context.device.updateDescriptorSets(write_descriptor_sets, {});
		}

		indirect_buffers = indirect_resource.ref();
	}
}
