#include "render/pipeline/composite.hpp"
#include "common/util/construct.hpp"
#include "render/pipeline/util/fullscreen-pipeline.hpp"
#include "shader/composite.hpp"
#include "vulkan/util/glm.hpp"
#include "vulkan/util/linked-struct.hpp"
#include "vulkan/util/pool-size.hpp"
#include "vulkan/util/shader.hpp"

namespace render
{
	namespace
	{
		consteval auto get_resource_layout_bindings() noexcept
		{
			constexpr auto exposure_result_binding = vk::DescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment
			};

			constexpr auto hdr_image_binding = vk::DescriptorSetLayoutBinding{
				.binding = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment
			};

			return std::to_array({exposure_result_binding, hdr_image_binding});
		}

		constexpr auto RESOURCE_BINDINGS = get_resource_layout_bindings();
	}

	std::expected<CompositePipeline, Error> CompositePipeline::create(
		const vulkan::DeviceContext& context,
		vk::Format target_format
	) noexcept
	{
		/*===== Descriptor Set Layout =====*/

		auto descriptor_set_layout_result =
			context.device
				.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo().setBindings(RESOURCE_BINDINGS))
				.transform_error(Error::from<vk::Result>());
		if (!descriptor_set_layout_result)
			return descriptor_set_layout_result.error().forward("Create descriptor set layout failed");
		auto descriptor_set_layout = std::move(*descriptor_set_layout_result);

		/*===== Pipeline Layout =====*/

		const auto layouts = std::to_array<vk::DescriptorSetLayout>({descriptor_set_layout});
		auto pipeline_layout_result =
			context.device.createPipelineLayout(vk::PipelineLayoutCreateInfo().setSetLayouts(layouts))
				.transform_error(Error::from<vk::Result>());
		if (!pipeline_layout_result)
			return pipeline_layout_result.error().forward("Create pipeline layout failed");
		auto pipeline_layout = std::move(*pipeline_layout_result);

		/*===== Shader Modules =====*/

		auto vertex_shader_result = fullscreen::get_vertex_shader(context.device);
		auto fragment_shader_result = vulkan::create_shader(context.device, shader::composite);

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

		const auto color_attachment_blend_states = std::to_array({fullscreen::BLEND_STATE});
		const auto color_blend_state =
			vk::PipelineColorBlendStateCreateInfo().setAttachments(color_attachment_blend_states);

		const auto pipeline_rendering_create_info =
			vk::PipelineRenderingCreateInfo().setColorAttachmentFormats(target_format);

		vulkan::LinkedStruct<vk::GraphicsPipelineCreateInfo> pipeline_create_info =
			vk::GraphicsPipelineCreateInfo()
				.setStages(shader_stages)
				.setPVertexInputState(&fullscreen::VERTEX_INPUT_STATE)
				.setPInputAssemblyState(&fullscreen::INPUT_ASSEMBLY_STATE)
				.setPRasterizationState(&fullscreen::RASTERIZATION_STATE)
				.setPMultisampleState(&fullscreen::MULTISAMPLE_STATE)
				.setPDepthStencilState(&fullscreen::DEPTH_STENCIL_STATE)
				.setPColorBlendState(&color_blend_state)
				.setPViewportState(&fullscreen::VIEWPORT_STATE)
				.setPDynamicState(&fullscreen::DYNAMIC_STATE_INFO)
				.setLayout(pipeline_layout);
		pipeline_create_info.push(pipeline_rendering_create_info);

		auto pipeline_result =
			context.device.createGraphicsPipeline(nullptr, pipeline_create_info.get())
				.transform_error(Error::from<vk::Result>());
		if (!pipeline_result) return pipeline_result.error().forward("Create graphics pipeline failed");
		auto pipeline = std::move(*pipeline_result);

		/*===== Sampler =====*/

		constexpr auto sampler_create_info = vk::SamplerCreateInfo{
			.magFilter = vk::Filter::eNearest,
			.minFilter = vk::Filter::eNearest,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,

			// NOTE: use repeat to flip image vertically
			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.mipLodBias = 0.0f,
			.minLod = 0.0f,
			.maxLod = 0.0f,
		};

		auto sampler_result =
			context.device.createSampler(sampler_create_info).transform_error(Error::from<vk::Result>());
		if (!sampler_result) return sampler_result.error().forward("Create sampler");
		auto sampler = std::move(*sampler_result);

		return CompositePipeline(
			std::move(descriptor_set_layout),
			std::move(pipeline_layout),
			std::move(pipeline),
			std::move(sampler)
		);
	}

	std::expected<std::vector<CompositePipeline::ResourceSet>, Error> CompositePipeline::create_resource_sets(
		const vulkan::DeviceContext& context,
		uint32_t count
	) const noexcept
	{
		const auto pool_sizes = vulkan::calc_pool_sizes(RESOURCE_BINDINGS, count);

		auto descriptor_pool_result =
			context.device
				.createDescriptorPool(
					vk::DescriptorPoolCreateInfo()
						.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
						.setMaxSets(count)
						.setPoolSizes(pool_sizes)
				)
				.transform_error(Error::from<vk::Result>());
		if (!descriptor_pool_result) return descriptor_pool_result.error().forward("Create descriptor pool");
		auto descriptor_pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*descriptor_pool_result));

		const auto layouts = std::vector(count, *resource_layout);
		const auto set_alloc_info =
			vk::DescriptorSetAllocateInfo().setDescriptorPool(*descriptor_pool).setSetLayouts(layouts);
		auto sets_result =
			context.device.allocateDescriptorSets(set_alloc_info).transform_error(Error::from<vk::Result>());
		if (!sets_result) return sets_result.error().forward("Allocate descriptor sets");
		auto sets = std::move(*sets_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   std::views::repeat(descriptor_pool),
				   sets | std::views::as_rvalue,
				   std::views::repeat(*input_sampler)
			   )
			| std::ranges::to<std::vector>();
	}

	void CompositePipeline::render(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_binding
	) const noexcept
	{
		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*pipeline_layout,
			0,
			{resource_binding.descriptor_set},
			{}
		);

		command_buffer.setViewport(
			0,
			vk::Viewport{
				.x = 0.0f,
				.y = 0.0f,
				.width = static_cast<float>(resource_binding.image_size.x),
				.height = static_cast<float>(resource_binding.image_size.y),
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			}
		);

		const auto scissor = vk::Rect2D{
			.offset = vk::Offset2D{.x = 0, .y = 0},
			.extent = vulkan::to<vk::Extent2D>(resource_binding.image_size),
		};
		command_buffer.setScissor(0, scissor);

		command_buffer.draw(6, 1, 0, 0);
	}

	void CompositePipeline::ResourceSet::update(
		const vulkan::DeviceContext& context,
		const AutoExposureResource& exposure_resource,
		const ForwardRenderResource& forward_resource,
		glm::u32vec2 image_size
	) noexcept
	{
		this->image_size = image_size;

		const auto exposure_result_buffer_info = vk::DescriptorBufferInfo{
			.buffer = exposure_resource->exposure_result_buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const auto hdr_image_info = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = forward_resource->hdr.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		const auto binding_0 = vk::WriteDescriptorSet{
			.dstSet = descriptor_set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &exposure_result_buffer_info
		};
		const auto binding_1 = vk::WriteDescriptorSet{
			.dstSet = descriptor_set,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &hdr_image_info
		};

		const auto writes = std::to_array({binding_0, binding_1});
		context.device.updateDescriptorSets(writes, {});
	}
}
