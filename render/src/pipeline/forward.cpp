#include "render/pipeline/forward.hpp"
#include "shader/forward.hpp"
#include "vulkan/util/glm.hpp"
#include "vulkan/util/linked-struct.hpp"
#include "vulkan/util/pool-size.hpp"
#include "vulkan/util/shader.hpp"

#include <ranges>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	static consteval auto get_descriptor_set_bindings() noexcept
	{
		constexpr auto primitive_attr_buffer_binding = vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eFragment
		};

		constexpr auto indirect_buffer_binding = vk::DescriptorSetLayoutBinding{
			.binding = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eVertex
		};

		constexpr auto transform_buffer_binding = vk::DescriptorSetLayoutBinding{
			.binding = 2,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eVertex
		};

		constexpr auto camera_buffer_binding = vk::DescriptorSetLayoutBinding{
			.binding = 3,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
		};

		constexpr auto direct_light_param_binding = vk::DescriptorSetLayoutBinding{
			.binding = 4,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eFragment
		};

		return std::to_array({
			primitive_attr_buffer_binding,
			indirect_buffer_binding,
			transform_buffer_binding,
			camera_buffer_binding,
			direct_light_param_binding,
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
		vk::DescriptorSetLayout material_descriptor_set_layout,
		vk::DescriptorSetLayout data_descriptor_set_layout
	) noexcept
	{
		const auto set_layouts = std::to_array<const vk::DescriptorSetLayout>({
			material_descriptor_set_layout,
			data_descriptor_set_layout,
		});
		const auto create_info = vk::PipelineLayoutCreateInfo().setSetLayouts(set_layouts);

		auto layout_result =
			context.device.createPipelineLayout(create_info).transform_error(Error::from<vk::Result>());
		if (!layout_result) return layout_result.error().forward("Create pipeline layout failed");
		return std::move(*layout_result);
	}

	static constexpr auto get_vertex_input_attribute_descs() noexcept
	{
		constexpr auto vertex_position_attr_desc = vk::VertexInputAttributeDescription{
			.location = 0,
			.binding = 0,
			.format = vk::Format::eR32G32B32Sfloat,
			.offset = offsetof(model::FullVertex, position)
		};

		constexpr auto vertex_texcoord_attr_desc = vk::VertexInputAttributeDescription{
			.location = 1,
			.binding = 0,
			.format = vk::Format::eR32G32Sfloat,
			.offset = offsetof(model::FullVertex, texcoord)
		};

		constexpr auto vertex_normal_attr_desc = vk::VertexInputAttributeDescription{
			.location = 2,
			.binding = 0,
			.format = vk::Format::eR32G32B32Sfloat,
			.offset = offsetof(model::FullVertex, normal)
		};

		constexpr auto vertex_tangent_attr_desc = vk::VertexInputAttributeDescription{
			.location = 3,
			.binding = 0,
			.format = vk::Format::eR32G32B32A32Sfloat,
			.offset = offsetof(model::FullVertex, tangent)
		};

		return std::to_array({
			vertex_position_attr_desc,
			vertex_texcoord_attr_desc,
			vertex_normal_attr_desc,
			vertex_tangent_attr_desc,
		});
	}

	std::expected<vk::raii::Pipeline, Error> ForwardPipeline::create_pipeline(
		const vulkan::DeviceContext& context,
		const vk::raii::PipelineLayout& pipeline_layout,
		const vk::raii::ShaderModule& shader_module,
		vk::Format color_format,
		vk::Format depth_format,
		bool alpha_mask_enabled,
		bool double_sided
	) noexcept
	{
		/*===== Shaders =====*/

		const auto spec_data = SpecializationConstant{
			.alpha_mask_enabled = alpha_mask_enabled ? vk::True : vk::False,
			.double_sided = double_sided ? vk::True : vk::False
		};

		const auto alpha_mask_spec_entry = vk::SpecializationMapEntry{
			.constantID = 0,
			.offset = offsetof(SpecializationConstant, alpha_mask_enabled),
			.size = sizeof(vk::Bool32),
		};
		const auto double_sided_spec_entry = vk::SpecializationMapEntry{
			.constantID = 1,
			.offset = offsetof(SpecializationConstant, double_sided),
			.size = sizeof(vk::Bool32),
		};
		const auto spec_entries = std::to_array({
			alpha_mask_spec_entry,
			double_sided_spec_entry,
		});

		const auto specialization_info =
			vk::SpecializationInfo().setMapEntries(spec_entries).setData<SpecializationConstant>(spec_data);

		const auto vertex_shader_stage_create_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = shader_module,
			.pName = "main_vertex",
		};
		const auto fragment_shader_stage_create_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = shader_module,
			.pName = "main_fragment",
			.pSpecializationInfo = &specialization_info
		};

		const auto shader_stage_create_infos = std::to_array({
			vertex_shader_stage_create_info,
			fragment_shader_stage_create_info,
		});

		/*===== Input =====*/

		constexpr auto vertex_input_binding_desc = vk::VertexInputBindingDescription{
			.binding = 0,
			.stride = sizeof(model::FullVertex),
			.inputRate = vk::VertexInputRate::eVertex
		};

		constexpr auto vertex_input_attribute_descs = get_vertex_input_attribute_descs();

		const auto vertex_input_state_create_info =
			vk::PipelineVertexInputStateCreateInfo()
				.setVertexBindingDescriptions(vertex_input_binding_desc)
				.setVertexAttributeDescriptions(vertex_input_attribute_descs);

		/*===== Fixed Function =====*/

		const auto input_assembly_state_create_info = vk::PipelineInputAssemblyStateCreateInfo{
			.topology = vk::PrimitiveTopology::eTriangleList,
			.primitiveRestartEnable = vk::False
		};

		const auto rasterization_info = vk::PipelineRasterizationStateCreateInfo{
			.cullMode = double_sided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack,
			.lineWidth = 1.0
		};

		constexpr auto multisample_info =
			vk::PipelineMultisampleStateCreateInfo{.rasterizationSamples = vk::SampleCountFlagBits::e1};

		constexpr auto depth_stencil_info = vk::PipelineDepthStencilStateCreateInfo{
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eGreater,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False
		};

		const auto swapchain_attachment_blend_state = vk::PipelineColorBlendAttachmentState{
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA
		};
		const auto color_attachment_blend_states = std::to_array({
			swapchain_attachment_blend_state,
		});
		const auto color_blend_info =
			vk::PipelineColorBlendStateCreateInfo().setAttachments(color_attachment_blend_states);

		/*===== Output =====*/

		const auto pipeline_rendering_create_info =
			vk::PipelineRenderingCreateInfo()
				.setColorAttachmentFormats(color_format)
				.setDepthAttachmentFormat(depth_format);

		/* Viewport */

		const auto viewport_info = vk::PipelineViewportStateCreateInfo{
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr,
		};

		/*===== Dynamic States =====*/

		const auto dynamic_states = std::to_array({
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor,
		});
		const auto dynamic_state_info = vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamic_states);

		/*===== Pipeline Creation =====*/

		vulkan::LinkedStruct<vk::GraphicsPipelineCreateInfo> pipeline_create_info =
			vk::GraphicsPipelineCreateInfo()
				.setStages(shader_stage_create_infos)
				.setPVertexInputState(&vertex_input_state_create_info)
				.setPInputAssemblyState(&input_assembly_state_create_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&multisample_info)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&color_blend_info)
				.setPViewportState(&viewport_info)
				.setPDynamicState(&dynamic_state_info)
				.setLayout(pipeline_layout);
		pipeline_create_info.push(pipeline_rendering_create_info);

		auto pipeline_result =
			context.device.createGraphicsPipeline(nullptr, pipeline_create_info.get())
				.transform_error(Error::from<vk::Result>());
		if (!pipeline_result)
		{
			return pipeline_result.error().forward(
				"Create graphics pipeline failed",
				std::format("alpha_mask_enabled={}, double_sided={}", alpha_mask_enabled, double_sided)
			);
		}

		return std::move(*pipeline_result);
	}

	std::expected<ForwardPipeline, Error> ForwardPipeline::create(
		const vulkan::DeviceContext& context,
		const render::MaterialLayout& material_layout,
		vk::Format color_format,
		vk::Format depth_format
	) noexcept
	{
		auto shader_module_result = vulkan::create_shader(context.device, shader::forward);
		if (!shader_module_result) return shader_module_result.error().forward("Create shader module failed");
		auto shader_module = std::move(*shader_module_result);

		auto descriptor_set_layout_result = create_descriptor_set_layout(context);
		if (!descriptor_set_layout_result)
		{
			return descriptor_set_layout_result.error().forward("Create descriptor set layout failed");
		}
		auto data_descriptor_set_layout = std::move(*descriptor_set_layout_result);

		auto pipeline_layout_result =
			create_pipeline_layout(context, material_layout.layout, data_descriptor_set_layout);
		if (!pipeline_layout_result)
		{
			return pipeline_layout_result.error().forward("Create pipeline layout failed");
		}
		auto pipeline_layout = std::move(*pipeline_layout_result);

		auto pipelines_result = PerRenderState<vk::raii::Pipeline>::from_ctor(
			[&](model::AlphaMode alpha_mode, bool double_sided) {
				return create_pipeline(
					context,
					pipeline_layout,
					shader_module,
					color_format,
					depth_format,
					alpha_mode == model::AlphaMode::Mask,
					double_sided
				);
			}
		);
		if (!pipelines_result)
		{
			return pipelines_result.error().forward("Create pipelines failed");
		}
		auto pipelines = std::move(*pipelines_result);

		return ForwardPipeline{
			std::move(data_descriptor_set_layout),
			std::move(pipeline_layout),
			std::move(pipelines)
		};
	}

	std::expected<std::vector<ForwardPipeline::ResourceSet>, Error> ForwardPipeline::create_resource_sets(
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

		const auto layouts = std::vector(SETS_PER_RESOURCE_SET, *data_descriptor_set_layout);
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

	void ForwardPipeline::render(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		/*===== Pre-rendering Layout Transitions =====*/

		const auto pre_depth_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests
				| vk::PipelineStageFlagBits2::eLateFragmentTests,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests
				| vk::PipelineStageFlagBits2::eLateFragmentTests,
			.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite
				| vk::AccessFlagBits2::eDepthStencilAttachmentRead,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.image = resource_set.depth_target,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eDepth)
		};

		const auto pre_color_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.image = resource_set.color_target,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		const auto pre_barriers = std::to_array({
			pre_depth_barrier,
			pre_color_barrier,
		});
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barriers));

		/*===== Draw =====*/

		const auto swapchain_attachment_info = vk::RenderingAttachmentInfo{
			.imageView = resource_set.color_target_view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
		};

		const auto depth_attachment_info = vk::RenderingAttachmentInfo{
			.imageView = resource_set.depth_target_view,
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = vk::ClearDepthStencilValue{.depth = 0.0f, .stencil = 0}
		};

		const auto rendering_rect = vk::Rect2D{
			.offset = vk::Offset2D{.x = 0, .y = 0},
			.extent = vulkan::to<vk::Extent2D>(resource_set.rendering_extent)
		};

		const auto rendering_info =
			vk::RenderingInfo{
				.renderArea = rendering_rect,
				.layerCount = 1,
			}
				.setColorAttachments(swapchain_attachment_info)
				.setPDepthAttachment(&depth_attachment_info);

		command_buffer.beginRendering(rendering_info);

		command_buffer.bindVertexBuffers(0, static_cast<vk::Buffer>(resource_set.vertex_buffer), {0});
		command_buffer.bindIndexBuffer(resource_set.index_buffer, 0, vk::IndexType::eUint32);

		command_buffer.setViewport(
			0,
			vk::Viewport{
				.x = 0.0f,
				.y = 0.0f,
				.width = static_cast<float>(resource_set.rendering_extent.x),
				.height = static_cast<float>(resource_set.rendering_extent.y),
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			}
		);
		command_buffer.setScissor(0, rendering_rect);

		for (const auto& [pipeline, data_descriptor_set, indirect_buffer] : std::views::zip(
				 pipelines.all(),
				 resource_set.data_descriptor_set.all(),
				 resource_set.indirect_buffers.all()
			 ))
		{
			if (indirect_buffer.count() == 0) continue;

			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				*pipeline_layout,
				0,
				{resource_set.material_descriptor_set, data_descriptor_set},
				{}
			);

			command_buffer.drawIndexedIndirect(
				indirect_buffer,
				0,
				indirect_buffer.count(),
				sizeof(render::IndirectDrawcall)
			);
		}

		command_buffer.endRendering();

		/*===== Post-rendering Layout Transitions =====*/

		const auto post_color_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask =
				vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.image = resource_set.color_target,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_color_barrier));
	}

	void ForwardPipeline::ResourceSet::update(
		const vulkan::DeviceContext& context,
		const Model& model,
		const vulkan::ElementBufferRef<Camera>& camera_param,
		const vulkan::ElementBufferRef<DirectLight>& primary_light_param,
		const HostDrawcallResource& host_drawcall,
		const IndirectResource& indirect_resource,
		const ForwardRenderResource& forward_resource,
		glm::u32vec2 extent
	) noexcept
	{
		/* Write descriptor sets */

		const auto primitive_attr_buffer_write = vk::DescriptorBufferInfo{
			.buffer = model.mesh_list->primitive_attr_buffer,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto indirect_buffer_writes = indirect_resource.ref().map([](const auto& drawcall_buffer) {
			return vk::DescriptorBufferInfo{
				.buffer = drawcall_buffer,
				.offset = 0,
				.range = drawcall_buffer.size_vk(),
			};
		});

		const auto transform_buffer_write = vk::DescriptorBufferInfo{
			.buffer = host_drawcall->transform,
			.offset = 0,
			.range = host_drawcall->transform.size_vk(),
		};

		const auto camera_buffer_write = vk::DescriptorBufferInfo{
			.buffer = camera_param,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto direct_light_param_buffer_write = vk::DescriptorBufferInfo{
			.buffer = primary_light_param,
			.offset = 0,
			.range = vk::WholeSize,
		};

		for (const auto& [descriptor_set, indirect_buffer_write] :
			 std::views::zip(data_descriptor_set.all(), indirect_buffer_writes.all()))
		{
			const auto primitive_attr_buffer_write_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &primitive_attr_buffer_write
			};

			const auto indirect_buffer_write_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &indirect_buffer_write
			};

			const auto transform_buffer_write_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &transform_buffer_write
			};

			const auto camera_buffer_write_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &camera_buffer_write
			};

			const auto direct_light_param_buffer_write_set = vk::WriteDescriptorSet{
				.dstSet = descriptor_set,
				.dstBinding = 4,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &direct_light_param_buffer_write
			};

			const auto write_sets = std::to_array({
				primitive_attr_buffer_write_set,
				indirect_buffer_write_set,
				transform_buffer_write_set,
				camera_buffer_write_set,
				direct_light_param_buffer_write_set,
			});

			context.device.updateDescriptorSets(write_sets, {});
		}

		/* Store infos */

		material_descriptor_set = model.material_list.get_descriptor_set();

		vertex_buffer = model.mesh_list->vertex_buffer;
		index_buffer = model.mesh_list->index_buffer;
		indirect_buffers = indirect_resource.ref();

		rendering_extent = extent;
		color_target = forward_resource->hdr.image;
		depth_target = forward_resource->depth.image;
		color_target_view = forward_resource->hdr.view;
		depth_target_view = forward_resource->depth.view;
	}
}
