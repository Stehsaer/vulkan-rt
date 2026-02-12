#include "pipeline.hpp"
#include "model.hpp"

#include "shader/object.hpp"
#include "vulkan/util/linked-struct.hpp"
#include "vulkan/util/shader.hpp"

ObjectRenderPipeline ObjectRenderPipeline::create(const vulkan::Context& context)
{
	/* Pipeline Layout */

	const auto push_constant_range = vk::PushConstantRange{
		.stageFlags = vk::ShaderStageFlagBits::eVertex,
		.offset = 0,
		.size = sizeof(PushConstant)
	};

	const auto pipeline_layout_create_info =
		vk::PipelineLayoutCreateInfo{}.setPushConstantRanges(push_constant_range);
	auto pipeline_layout =
		context.device.createPipelineLayout(pipeline_layout_create_info)
			.transform_error(Error::from<vk::Result>())
		| Error::unwrap("Create pipeline layout failed");

	/* Shader Module */

	auto shader_module = vulkan::util::create_shader(context.device, shader::object)
		| Error::unwrap("Create shader module failed");

	/* Pipeline */

	// == Shader ==

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

	// == Dynamic State ==

	const auto dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
	const auto dynamic_state_info = vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamic_states);

	const auto viewport_info = vk::PipelineViewportStateCreateInfo{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	// == Input ==

	const auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	const auto position_attribute = vk::VertexInputAttributeDescription{
		.location = 0,
		.binding = 0,
		.format = vk::Format::eR32G32B32Sfloat,
		.offset = offsetof(Vertex, position)
	};
	const auto normal_attribute = vk::VertexInputAttributeDescription{
		.location = 1,
		.binding = 0,
		.format = vk::Format::eR32G32B32Sfloat,
		.offset = offsetof(Vertex, normal)
	};
	const auto uv_attribute = vk::VertexInputAttributeDescription{
		.location = 2,
		.binding = 0,
		.format = vk::Format::eR32G32Sfloat,
		.offset = offsetof(Vertex, uv)
	};

	const auto vertex_attribute_descs = std::to_array<vk::VertexInputAttributeDescription>(
		{position_attribute, normal_attribute, uv_attribute}
	);

	const auto vertex_buffer_descs = std::to_array<vk::VertexInputBindingDescription>({
		{.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex}
	});

	const auto vertex_input_info =
		vk::PipelineVertexInputStateCreateInfo{}
			.setVertexBindingDescriptions(vertex_buffer_descs)
			.setVertexAttributeDescriptions(vertex_attribute_descs);

	// == Fixed-function ==

	const auto rasterization_info =
		vk::PipelineRasterizationStateCreateInfo{.cullMode = vk::CullModeFlagBits::eBack, .lineWidth = 1.0};
	const auto multisample_info =
		vk::PipelineMultisampleStateCreateInfo{.rasterizationSamples = vk::SampleCountFlagBits::e1};
	const auto depth_stencil_info = vk::PipelineDepthStencilStateCreateInfo{
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eGreater,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False
	};

	// == Output ==

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
		vk::PipelineRenderingCreateInfo()
			.setColorAttachmentFormats(attachment_formats)
			.setDepthAttachmentFormat(vk::Format::eD32Sfloat);

	vulkan::util::LinkedStruct<vk::GraphicsPipelineCreateInfo> graphics_create_info =
		vk::GraphicsPipelineCreateInfo{
			.pNext = &pipeline_rendering_info,
			.pVertexInputState = &vertex_input_info,
			.pInputAssemblyState = &input_assembly_info,
			.pViewportState = &viewport_info,
			.pRasterizationState = &rasterization_info,
			.pMultisampleState = &multisample_info,
			.pDepthStencilState = &depth_stencil_info,
			.pColorBlendState = &color_blend_info,
			.pDynamicState = &dynamic_state_info,
			.layout = *pipeline_layout
		}
			.setStages(shader_stages);
	graphics_create_info.push(pipeline_rendering_info);

	auto pipeline =
		context.device.createGraphicsPipeline(nullptr, graphics_create_info.get())
			.transform_error(Error::from<vk::Result>())
		| Error::unwrap("Create graphics pipeline failed");

	return ObjectRenderPipeline{.layout = std::move(pipeline_layout), .pipeline = std::move(pipeline)};
}
