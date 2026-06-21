#include "render/pipeline/deferred.hpp"
#include "common/util/array.hpp"
#include "common/util/error.hpp"
#include "model/material.hpp"
#include "model/mesh.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/indirect-drawcall.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"
#include "render/pipeline/util/constant.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/hdr.hpp"
#include "render/resource/host.hpp"
#include "render/resource/indirect.hpp"
#include "render/util/per-render-state.hpp"
#include "shader/deferred.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/container/host/linked-struct.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/numeric/glm.hpp"
#include "vulkan/numeric/pool-size.hpp"
#include "vulkan/util/descriptor-set-layout.hpp"
#include "vulkan/util/shader.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <glm/ext/vector_uint2_sized.hpp>
#include <libassert/assert.hpp>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	namespace
	{
		using DataDescriptorSetLayout = vulkan::MonoDescriptorSetLayout<
			// primitive_attr
			vulkan::
				MonoDescriptorSetSlot<vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment>,
			// indirect_buffer
			vulkan::
				MonoDescriptorSetSlot<vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex>,
			// transform_buffer
			vulkan::
				MonoDescriptorSetSlot<vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex>,
			// camera_buffer
			vulkan::MonoDescriptorSetSlot<
				vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eVertex,
				vk::ShaderStageFlagBits::eFragment
			>
		>;

		std::expected<vk::raii::PipelineLayout, Error> create_pipeline_layout(
			const vulkan::Context& context,
			vk::DescriptorSetLayout material_descriptor_set_layout,
			vk::DescriptorSetLayout data_descriptor_set_layout
		) noexcept
		{
			const auto set_layouts = std::to_array<const vk::DescriptorSetLayout>({
				material_descriptor_set_layout,
				data_descriptor_set_layout,
			});
			const auto create_info = vk::PipelineLayoutCreateInfo().setSetLayouts(set_layouts);

			auto layout_result = context.device.createPipelineLayout(create_info);
			if (!layout_result) return Error::from(layout_result);
			return std::move(*layout_result);
		}

		constexpr auto get_vertex_input_attribute_descs() noexcept
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
	}

	std::expected<vk::raii::Pipeline, Error> DeferredPipeline::create_pipeline(
		const vulkan::Context& context,
		vk::PipelineLayout pipeline_layout,
		vk::ShaderModule shader_module,
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

		const auto rasterization_info = vk::PipelineRasterizationStateCreateInfo{
			.cullMode = double_sided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack,
			.lineWidth = 1.0
		};

		constexpr auto depth_stencil_info = vk::PipelineDepthStencilStateCreateInfo{
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eGreater,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False
		};

		const auto attachment_blend_state = vk::PipelineColorBlendAttachmentState{
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA
		};
		const auto color_attachment_blend_states = std::to_array({
			attachment_blend_state,
			attachment_blend_state,
			attachment_blend_state,
			attachment_blend_state,
			attachment_blend_state,
		});
		const auto color_blend_info =
			vk::PipelineColorBlendStateCreateInfo().setAttachments(color_attachment_blend_states);

		/*===== Output =====*/

		constexpr auto color_formats = std::to_array({
			DeferredAttachment::ALBEDO_FORMAT,  // Location 0
			DeferredAttachment::NORMAL_FORMAT,  // Location 1
			DeferredAttachment::NORMAL_FORMAT,  // Location 2
			DeferredAttachment::PBR_FORMAT,     // Location 3
			HdrAttachment::HDR_FORMAT,          // Location 4
		});

		const auto pipeline_rendering_create_info =
			vk::PipelineRenderingCreateInfo()
				.setColorAttachmentFormats(color_formats)
				.setDepthAttachmentFormat(DeferredAttachment::DEPTH_FORMAT);

		/*===== Dynamic States =====*/

		const auto dynamic_state_info =
			vk::PipelineDynamicStateCreateInfo().setDynamicStates(constant::DYNAMIC_VIEWPORT_DYNSTATE);

		/*===== Pipeline Creation =====*/

		vulkan::LinkedStruct<vk::GraphicsPipelineCreateInfo> pipeline_create_info =
			vk::GraphicsPipelineCreateInfo()
				.setStages(shader_stage_create_infos)
				.setPVertexInputState(&vertex_input_state_create_info)
				.setPInputAssemblyState(&constant::TRIANGLE_LIST_INPUT_ASSEMBLY_STATE)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&constant::BASIC_MULTISAMPLE_STATE)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&color_blend_info)
				.setPViewportState(&constant::DYNAMIC_VIEWPORT_STATE)
				.setPDynamicState(&dynamic_state_info)
				.setLayout(pipeline_layout);
		pipeline_create_info.push(pipeline_rendering_create_info);

		auto pipeline_result = context.device.createGraphicsPipeline(nullptr, pipeline_create_info.get());
		if (!pipeline_result)
		{
			return Error::from(pipeline_result)
				.forward(
					"Create graphics pipeline failed",
					std::format("alpha_mask_enabled={}, double_sided={}", alpha_mask_enabled, double_sided)
				);
		}

		return std::move(*pipeline_result);
	}

	std::expected<DeferredPipeline, Error> DeferredPipeline::create(
		const vulkan::Context& context,
		const render::MaterialLayout& material_layout
	) noexcept
	{
		auto shader_module_result = vulkan::create_shader(context.device, shader::deferred);
		if (!shader_module_result)
			return shader_module_result.error().forward("Create primary shader module failed");
		auto shader_module = std::move(*shader_module_result);

		auto data_descriptor_set_layout_result =
			DataDescriptorSetLayout::create_descriptor_set_layout(context);
		if (!data_descriptor_set_layout_result)
			return data_descriptor_set_layout_result.error().forward(
				"Create data descriptor set layout failed"
			);
		auto data_descriptor_set_layout = std::move(*data_descriptor_set_layout_result);

		auto pipeline_layout_result =
			create_pipeline_layout(context, material_layout.layout, data_descriptor_set_layout);
		if (!pipeline_layout_result)
			return pipeline_layout_result.error().forward("Create primary pipeline layout failed");
		auto pipeline_layout = std::move(*pipeline_layout_result);

		auto pipelines_result = PerRenderState<vk::raii::Pipeline>::from_ctor(
			[&](model::AlphaMode alpha_mode, bool double_sided) {
				return create_pipeline(
					context,
					pipeline_layout,
					shader_module,
					alpha_mode == model::AlphaMode::Mask,
					double_sided
				);
			}
		);
		if (!pipelines_result) return pipelines_result.error().forward("Create pipelines failed");
		auto pipelines = std::move(*pipelines_result);

		return DeferredPipeline{
			std::move(data_descriptor_set_layout),
			std::move(pipeline_layout),
			std::move(pipelines)
		};
	}

	std::expected<std::vector<DeferredPipeline::ResourceSet>, Error> DeferredPipeline::create_resource_sets(
		const vulkan::Context& context,
		uint32_t count
	) const noexcept
	{
		/*
		 * NOTE: Each resource set contains 4 descriptor sets for each material variant
		 */

		static constexpr auto SETS_PER_RESOURCE_SET = 4;

		const auto descriptor_bindings = DataDescriptorSetLayout::get_bindings();
		const auto descriptor_pool_sizes =
			vulkan::calc_pool_sizes(descriptor_bindings, count * SETS_PER_RESOURCE_SET);

		auto descriptor_pool_result = context.device.createDescriptorPool(
			vk::DescriptorPoolCreateInfo()
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
				.setMaxSets(count * SETS_PER_RESOURCE_SET)
				.setPoolSizes(descriptor_pool_sizes)
		);
		if (!descriptor_pool_result) return Error::from(descriptor_pool_result);
		auto descriptor_pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*descriptor_pool_result));

		const auto data_layouts = std::vector(SETS_PER_RESOURCE_SET, *data_descriptor_set_layout);
		const auto data_set_alloc_info =
			vk::DescriptorSetAllocateInfo().setDescriptorPool(*descriptor_pool).setSetLayouts(data_layouts);

		const auto create_resource_set_fn = [&] -> std::expected<ResourceSet, Error> {
			auto sets_result = context.device.allocateDescriptorSets(data_set_alloc_info);
			if (!sets_result) return Error::from(sets_result);
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

	void DeferredPipeline::render(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.resource.has_value());

		const auto color_attachments = std::to_array({
			resource_set.resource->attachment.albedo,
			resource_set.resource->attachment.normal,
			resource_set.resource->attachment.geom_normal,
			resource_set.resource->attachment.pbr,
			resource_set.resource->attachment.hdr,
		});

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
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = resource_set.resource->attachment.depth.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eDepth)
		};

		const auto pre_color_barriers =
			color_attachments | util::map_array([](const vulkan::AttachmentView& attachment) {
				return vk::ImageMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					.srcAccessMask = vk::AccessFlagBits2::eNone,
					.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
					.oldLayout = vk::ImageLayout::eUndefined,
					.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
					.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					.image = attachment.image,
					.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
				};
			});

		const auto pre_barriers = util::array_concat(pre_depth_barrier, pre_color_barriers);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barriers));

		/*===== Draw =====*/

		const auto color_attachment_infos =
			color_attachments | util::map_array([](const vulkan::AttachmentView& attachment) {
				return vk::RenderingAttachmentInfo{
					.imageView = attachment.view,
					.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
					.loadOp = vk::AttachmentLoadOp::eClear,
					.storeOp = vk::AttachmentStoreOp::eStore,
					.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
				};
			});

		const auto depth_attachment_info = vk::RenderingAttachmentInfo{
			.imageView = resource_set.resource->attachment.depth.view,
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearDepthStencilValue{.depth = 0.0f, .stencil = 0}
		};

		const auto rendering_rect = vk::Rect2D{
			.offset = vk::Offset2D{.x = 0, .y = 0},
			.extent = vulkan::to<vk::Extent2D>(resource_set.resource->attachment.extent)
		};

		const auto rendering_info =
			vk::RenderingInfo()
				.setRenderArea(rendering_rect)
				.setLayerCount(1)
				.setColorAttachments(color_attachment_infos)
				.setPDepthAttachment(&depth_attachment_info);

		command_buffer.beginRendering(rendering_info);

		command_buffer
			.bindVertexBuffers(0, static_cast<vk::Buffer>(resource_set.resource->vertex_buffer), {0});
		command_buffer.bindIndexBuffer(resource_set.resource->index_buffer, 0, vk::IndexType::eUint32);

		command_buffer.setViewport(
			0,
			vk::Viewport{
				.x = 0.0f,
				.y = 0.0f,
				.width = static_cast<float>(resource_set.resource->attachment.extent.x),
				.height = static_cast<float>(resource_set.resource->attachment.extent.y),
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			}
		);
		command_buffer.setScissor(0, rendering_rect);

		for (
			const auto& [pipeline, data_descriptor_set, indirect_buffer] : std::views::zip(
				pipelines.all(),
				resource_set.data_descriptor_set.all(),
				resource_set.resource->indirect_buffers.all()
			)
		)
		{
			if (indirect_buffer.count() == 0) continue;

			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				*pipeline_layout,
				0,
				{resource_set->material_descriptor_set, data_descriptor_set},
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
		// NOTE: HDR attachment is handled differently than others -- no layout transitions, expect next usage
		// to be color attachment

		const auto post_layout_transition_color_attachments = std::to_array({
			resource_set.resource->attachment.albedo,
			resource_set.resource->attachment.normal,
			resource_set.resource->attachment.geom_normal,
			resource_set.resource->attachment.pbr,
		});

		const auto post_color_barriers =
			post_layout_transition_color_attachments
			| util::map_array([](const vulkan::AttachmentView& attachment) {
				  return vk::ImageMemoryBarrier2{
					  .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					  .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
					  .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader
						  | vk::PipelineStageFlagBits2::eComputeShader,
					  .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
					  .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
					  .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					  .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					  .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					  .image = attachment.image,
					  .subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
				  };
			  });

		const auto post_hdr_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = resource_set->attachment.hdr.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		const auto post_depth_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests
				| vk::PipelineStageFlagBits2::eLateFragmentTests,
			.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite
				| vk::AccessFlagBits2::eDepthStencilAttachmentRead,
			.dstStageMask =
				vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
			.oldLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = resource_set.resource->attachment.depth.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eDepth)
		};

		const auto post_barriers =
			util::array_concat(post_color_barriers, post_hdr_barrier, post_depth_barrier);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_barriers));
	}

	void DeferredPipeline::ResourceSet::update(
		const vulkan::Context& context,
		const Model& model,
		const HostDrawcallResource& host_drawcall,
		const IndirectResource& indirect_resource,
		DeferredAttachment::View deferred,
		HdrAttachment::View hdr,
		vulkan::ElementBufferRef<Camera> camera_param
	) noexcept
	{
		DEBUG_ASSERT(deferred.extent == hdr.extent);

		/*===== Storage Infos =====*/

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

		/*===== Write descriptor for primary pipeline =====*/

		for (
			const auto& [descriptor_set, indirect_buffer_write] :
			std::views::zip(data_descriptor_set.all(), indirect_buffer_writes.all())
		)
		{
			const auto write_sets = DataDescriptorSetLayout::get_write_infos(
				descriptor_set,
				primitive_attr_buffer_write,
				indirect_buffer_write,
				transform_buffer_write,
				camera_buffer_write
			);

			context.device.updateDescriptorSets(write_sets, {});
		}

		/*===== Store infos =====*/

		const auto attachment = Attachment{
			.extent = deferred.extent,
			.albedo = deferred.albedo,
			.normal = deferred.normal,
			.geom_normal = deferred.geom_normal,
			.pbr = deferred.pbr,
			.depth = deferred.depth,
			.hdr = hdr.attachment
		};

		resource = Resource{
			.material_descriptor_set = model.material_list.get_descriptor_set(),

			.vertex_buffer = model.mesh_list->vertex_buffer,
			.index_buffer = model.mesh_list->index_buffer,
			.indirect_buffers = indirect_resource.ref(),

			.attachment = attachment
		};
	}
}
