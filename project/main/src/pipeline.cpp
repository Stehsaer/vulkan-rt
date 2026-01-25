#include "pipeline.hpp"
#include "shader/first_shader.hpp"
#include "vertex.hpp"
#include "vulkan/util/linked-struct.hpp"
#include "vulkan/util/shader.hpp"

std::expected<Pipeline, Error> Pipeline::create(
	const vulkan::Context& context,
	vk::DescriptorSetLayout main_set_layout
) noexcept
{
	const auto pipeline_layout_create_info = vk::PipelineLayoutCreateInfo{}.setSetLayouts(main_set_layout);
	auto pipeline_layout_expected = context.device.createPipelineLayout(pipeline_layout_create_info);
	if (!pipeline_layout_expected) return Error("Create pipeline layout failed");
	auto pipeline_layout = std::move(*pipeline_layout_expected);

	auto shader_module_expected = vulkan::util::create_shader(context.device, shader::first_shader);
	if (!shader_module_expected) return shader_module_expected.error().forward("Create shader module failed");
	auto shader_module = std::move(*shader_module_expected);

	const auto vertex_stage_info = vk::PipelineShaderStageCreateInfo{
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = *shader_module,
		.pName = "main_vertex"
	};
	const auto fragment_stage_info = vk::PipelineShaderStageCreateInfo{
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = *shader_module,
		.pName = "main_fragment"
	};
	const auto shader_stages = std::to_array({vertex_stage_info, fragment_stage_info});

	const auto dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
	const auto dynamic_state_info = vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamic_states);

	const auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	const auto vertex_attribute_descs = std::to_array<vk::VertexInputAttributeDescription>({
		{
         .location = 0,
         .binding = 0,
         .format = vk::Format::eR32G32Sfloat,
         .offset = offsetof(Vertex, position),
		 },
		{
         .location = 1,
         .binding = 0,
         .format = vk::Format::eR32G32Sfloat,
         .offset = offsetof(Vertex,              uv),
		 }
	});

	const auto vertex_buffer_descs = std::to_array<vk::VertexInputBindingDescription>({
		{.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex}
	});

	const auto vertex_input_info =
		vk::PipelineVertexInputStateCreateInfo{}
			.setVertexBindingDescriptions(vertex_buffer_descs)
			.setVertexAttributeDescriptions(vertex_attribute_descs);

	const auto viewport_info = vk::PipelineViewportStateCreateInfo{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	const auto rasterization_info =
		vk::PipelineRasterizationStateCreateInfo{.cullMode = vk::CullModeFlagBits::eNone, .lineWidth = 1.0};
	const auto multisample_info =
		vk::PipelineMultisampleStateCreateInfo{.rasterizationSamples = vk::SampleCountFlagBits::e1};

	const auto swapchain_attachment_blend_state = vk::PipelineColorBlendAttachmentState{
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA
	};
	const auto color_attachment_blend_states = std::to_array({swapchain_attachment_blend_state});
	const auto color_blend_info =
		vk::PipelineColorBlendStateCreateInfo{}.setAttachments(color_attachment_blend_states);

	const auto attachment_formats = std::to_array({context.swapchain_layout.surface_format.format});
	const auto pipeline_rendering_info =
		vk::PipelineRenderingCreateInfo().setColorAttachmentFormats(attachment_formats);

	vulkan::util::LinkedStruct<vk::GraphicsPipelineCreateInfo> graphics_create_info =
		vk::GraphicsPipelineCreateInfo{
			.pNext = &pipeline_rendering_info,
			.pVertexInputState = &vertex_input_info,
			.pInputAssemblyState = &input_assembly_info,
			.pViewportState = &viewport_info,
			.pRasterizationState = &rasterization_info,
			.pMultisampleState = &multisample_info,
			.pColorBlendState = &color_blend_info,
			.pDynamicState = &dynamic_state_info,
			.layout = *pipeline_layout
		}
			.setStages(shader_stages);
	graphics_create_info.push(
		vk::PipelineRenderingCreateInfo{}.setColorAttachmentFormats(attachment_formats)
	);

	auto pipeline_expected = context.device.createGraphicsPipeline(nullptr, graphics_create_info.get());
	if (!pipeline_expected) return Error("Create graphics pipeline failed");
	auto pipeline = std::move(*pipeline_expected);

	return Pipeline{.layout = std::move(pipeline_layout), .pipeline = std::move(pipeline)};
}