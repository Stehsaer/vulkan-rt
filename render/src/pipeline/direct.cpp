#include "render/pipeline/direct.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/direct-light.hpp"
#include "render/pipeline/util/constant.hpp"
#include "render/pipeline/util/fullscreen-pipeline.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/hdr.hpp"
#include "render/resource/shadow.hpp"
#include "shader/direct.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/container/host/linked-struct.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/glm.hpp"
#include "vulkan/numeric/pool-size.hpp"
#include "vulkan/util/shader.hpp"

#include <array>
#include <cstdint>
#include <expected>
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
		consteval auto get_resource_bindings() noexcept
		{
			constexpr auto albedo_tex_binding = vk::DescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
			};

			constexpr auto normal_tex_binding = vk::DescriptorSetLayoutBinding{
				.binding = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
			};

			constexpr auto pbr_tex_binding = vk::DescriptorSetLayoutBinding{
				.binding = 2,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
			};

			constexpr auto depth_tex_binding = vk::DescriptorSetLayoutBinding{
				.binding = 3,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
			};

			constexpr auto shadow_tex_binding = vk::DescriptorSetLayoutBinding{
				.binding = 4,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
			};

			constexpr auto camera_binding = vk::DescriptorSetLayoutBinding{
				.binding = 5,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
			};

			constexpr auto light_binding = vk::DescriptorSetLayoutBinding{
				.binding = 6,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
			};

			return std::to_array({
				albedo_tex_binding,
				normal_tex_binding,
				pbr_tex_binding,
				depth_tex_binding,
				shadow_tex_binding,
				camera_binding,
				light_binding,
			});
		}

		constexpr auto RESOURCE_BINDINGS = get_resource_bindings();
	}

	[[nodiscard]]
	std::expected<DirectLightingPipeline, Error> DirectLightingPipeline::create(
		const vulkan::Context& context
	) noexcept
	{
		/*===== Descriptor Set Layout =====*/

		auto descriptor_set_layout_result = context.device.createDescriptorSetLayout(
			vk::DescriptorSetLayoutCreateInfo().setBindings(RESOURCE_BINDINGS)
		);
		if (!descriptor_set_layout_result) return Error::from(descriptor_set_layout_result);
		auto descriptor_set_layout = std::move(*descriptor_set_layout_result);

		/*===== Pipeline Layout =====*/

		const auto layouts = std::to_array<vk::DescriptorSetLayout>({descriptor_set_layout});
		auto pipeline_layout_result =
			context.device.createPipelineLayout(vk::PipelineLayoutCreateInfo().setSetLayouts(layouts));
		if (!pipeline_layout_result) return Error::from(pipeline_layout_result);
		auto pipeline_layout = std::move(*pipeline_layout_result);

		/*===== Shader Modules =====*/

		auto vertex_shader_result = fullscreen::get_vertex_shader(context.device);
		auto fragment_shader_result = vulkan::create_shader(context.device, shader::direct);

		if (!vertex_shader_result) return vertex_shader_result.error().forward("Create vertex shader failed");
		if (!fragment_shader_result)
			return fragment_shader_result.error().forward("Create fragment shader failed");

		auto vertex_shader = std::move(*vertex_shader_result);
		auto fragment_shader = std::move(*fragment_shader_result);

		const auto vertex_shader_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = vertex_shader,
			.pName = "main"
		};
		const auto fragment_shader_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = fragment_shader,
			.pName = "main"
		};

		const auto shader_stages = std::to_array({
			vertex_shader_info,
			fragment_shader_info,
		});

		/*===== Pipeline =====*/

		const auto color_blend_state =
			vk::PipelineColorBlendStateCreateInfo().setAttachments(constant::ADDITIVE_BLEND_STATE);

		const auto pipeline_rendering_create_info =
			vk::PipelineRenderingCreateInfo().setColorAttachmentFormats(HdrAttachment::HDR_FORMAT);

		vulkan::LinkedStruct<vk::GraphicsPipelineCreateInfo> pipeline_create_info =
			vk::GraphicsPipelineCreateInfo()
				.setStages(shader_stages)
				.setPVertexInputState(&fullscreen::VERTEX_INPUT_STATE)
				.setPInputAssemblyState(&constant::TRIANGLE_LIST_INPUT_ASSEMBLY_STATE)
				.setPRasterizationState(&constant::NO_CULL_RASTERIZATION_STATE)
				.setPMultisampleState(&constant::BASIC_MULTISAMPLE_STATE)
				.setPDepthStencilState(&constant::NO_DEPTH_TEST_STATE)
				.setPColorBlendState(&color_blend_state)
				.setPViewportState(&constant::DYNAMIC_VIEWPORT_STATE)
				.setPDynamicState(&fullscreen::DYNAMIC_STATE_INFO)
				.setLayout(pipeline_layout);
		pipeline_create_info.push(pipeline_rendering_create_info);

		auto pipeline_result = context.device.createGraphicsPipeline(nullptr, pipeline_create_info.get());
		if (!pipeline_result) return Error::from(pipeline_result);
		auto pipeline = std::move(*pipeline_result);

		/*===== Sampler =====*/

		constexpr auto sampler_create_info = vk::SamplerCreateInfo{
			.magFilter = vk::Filter::eNearest,
			.minFilter = vk::Filter::eNearest,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,

			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eClampToEdge,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.mipLodBias = 0.0f,
			.minLod = 0.0f,
			.maxLod = 0.0f,
		};

		auto sampler_result = context.device.createSampler(sampler_create_info);
		if (!sampler_result) return Error::from(sampler_result);
		auto sampler = std::move(*sampler_result);

		constexpr auto shadow_sampler_create_info = vk::SamplerCreateInfo{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,

			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eClampToEdge,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.mipLodBias = 0.0f,
			.minLod = 0.0f,
			.maxLod = 0.0f,
		};

		auto shadow_sampler_result = context.device.createSampler(shadow_sampler_create_info);
		if (!shadow_sampler_result) return Error::from(shadow_sampler_result);
		auto shadow_sampler = std::move(*shadow_sampler_result);

		return DirectLightingPipeline(
			std::move(descriptor_set_layout),
			std::move(pipeline_layout),
			std::move(pipeline),
			std::move(sampler),
			std::move(shadow_sampler)
		);
	}

	std::expected<std::vector<DirectLightingPipeline::ResourceSet>, Error> DirectLightingPipeline::
		create_resource_sets(const vulkan::Context& context, uint32_t count) const noexcept
	{
		const auto pool_sizes = vulkan::calc_pool_sizes(RESOURCE_BINDINGS, count);

		auto descriptor_pool_result = context.device.createDescriptorPool(
			vk::DescriptorPoolCreateInfo()
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
				.setMaxSets(count)
				.setPoolSizes(pool_sizes)
		);
		if (!descriptor_pool_result) return Error::from(descriptor_pool_result);
		auto descriptor_pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*descriptor_pool_result));

		const auto layouts = std::vector(count, *descriptor_set_layout);
		const auto set_alloc_info =
			vk::DescriptorSetAllocateInfo().setDescriptorPool(*descriptor_pool).setSetLayouts(layouts);
		auto sets_result = context.device.allocateDescriptorSets(set_alloc_info);
		if (!sets_result) return Error::from(sets_result);
		auto sets = std::move(*sets_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   std::views::repeat(descriptor_pool),
				   sets | std::views::as_rvalue,
				   std::views::repeat(*sampler),
				   std::views::repeat(*shadow_sampler)
			   )
			| std::ranges::to<std::vector>();
	}

	void DirectLightingPipeline::render(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.resource.has_value());

		const auto extent = resource_set->hdr.extent;

		const auto scissor = vk::Rect2D{
			.offset = {.x = 0, .y = 0},
			.extent = vulkan::to<vk::Extent2D>(extent)
		};

		const auto viewport = vk::Viewport{
			.x = 0,
			.y = 0,
			.width = static_cast<float>(extent.x),
			.height = static_cast<float>(extent.y),
			.minDepth = 0,
			.maxDepth = 1,
		};

		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		command_buffer.setViewport(0, {viewport});
		command_buffer.setScissor(0, {scissor});
		command_buffer
			.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {resource_set.set}, {});
		command_buffer.draw(6, 1, 0, 0);
	}

	void DirectLightingPipeline::ResourceSet::update(
		const vulkan::Context& context,
		DeferredAttachment::View deferred,
		HdrAttachment::View hdr,
		ShadowAttachment::View shadow,
		vulkan::ElementBufferRef<Camera> camera,
		vulkan::ElementBufferRef<DirectLight> direct_light
	) noexcept
	{
		/*===== Texture / Buffer Infos =====*/

		const auto albedo_tex_info = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = deferred.albedo.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto normal_tex_info = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = deferred.normal.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto pbr_tex_info = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = deferred.pbr.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto depth_tex_info = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = deferred.depth.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto shadow_tex_info = vk::DescriptorImageInfo{
			.sampler = shadow_sampler,
			.imageView = shadow.shadow.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto camera_buf_info = vk::DescriptorBufferInfo{
			.buffer = camera,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto direct_light_buf_info = vk::DescriptorBufferInfo{
			.buffer = direct_light,
			.offset = 0,
			.range = vk::WholeSize,
		};

		/*===== Write Descriptor Set =====*/

		const auto albedo_tex_write_descriptor = vk::WriteDescriptorSet{
			.dstSet = set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &albedo_tex_info,
		};

		const auto normal_tex_write_descriptor = vk::WriteDescriptorSet{
			.dstSet = set,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &normal_tex_info,
		};

		const auto pbr_tex_write_descriptor = vk::WriteDescriptorSet{
			.dstSet = set,
			.dstBinding = 2,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &pbr_tex_info,
		};

		const auto depth_tex_write_descriptor = vk::WriteDescriptorSet{
			.dstSet = set,
			.dstBinding = 3,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &depth_tex_info,
		};

		const auto shadow_tex_write_descriptor = vk::WriteDescriptorSet{
			.dstSet = set,
			.dstBinding = 4,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &shadow_tex_info,
		};

		const auto camera_buf_write_descriptor = vk::WriteDescriptorSet{
			.dstSet = set,
			.dstBinding = 5,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &camera_buf_info,
		};

		const auto direct_light_buf_write_descriptor = vk::WriteDescriptorSet{
			.dstSet = set,
			.dstBinding = 6,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &direct_light_buf_info,
		};

		const auto write_descriptors = std::to_array({
			albedo_tex_write_descriptor,
			normal_tex_write_descriptor,
			pbr_tex_write_descriptor,
			depth_tex_write_descriptor,
			shadow_tex_write_descriptor,
			camera_buf_write_descriptor,
			direct_light_buf_write_descriptor,
		});

		context.device.updateDescriptorSets(write_descriptors, {});

		/*===== Store Persistent =====*/

		resource = Resource{.hdr = hdr};
	}
}
