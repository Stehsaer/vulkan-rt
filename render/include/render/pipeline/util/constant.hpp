#pragma once

#include <array>
#include <vulkan/vulkan.hpp>

namespace render::constant
{
	///
	/// @brief Triangle list input assembly
	///
	constexpr auto TRIANGLE_LIST_INPUT_ASSEMBLY_STATE = vk::PipelineInputAssemblyStateCreateInfo{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	///
	/// @brief No-cull rasterization state, suitable for full-screen pipelines
	///
	constexpr auto NO_CULL_RASTERIZATION_STATE = vk::PipelineRasterizationStateCreateInfo{
		.cullMode = vk::CullModeFlagBits::eNone,
		.lineWidth = 1.0,
	};

	///
	/// @brief Basic multisample state (1 sample)
	///
	constexpr auto BASIC_MULTISAMPLE_STATE =
		vk::PipelineMultisampleStateCreateInfo{.rasterizationSamples = vk::SampleCountFlagBits::e1};

	///
	/// @brief No-depth-test depth stencil state
	///
	constexpr auto NO_DEPTH_TEST_STATE = vk::PipelineDepthStencilStateCreateInfo{
		.depthTestEnable = vk::False,
		.depthWriteEnable = vk::False,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False
	};

	///
	/// @brief Default blend state (overwrite)
	///
	constexpr auto DEFAULT_BLEND_STATE = vk::PipelineColorBlendAttachmentState{
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA
	};

	///
	/// @brief Additive blend state, adds color channels and takes the maximum of alpha channel.
	///
	constexpr auto ADDITIVE_BLEND_STATE = vk::PipelineColorBlendAttachmentState{
		.blendEnable = vk::True,
		.srcColorBlendFactor = vk::BlendFactor::eOne,
		.dstColorBlendFactor = vk::BlendFactor::eOne,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eOne,
		.alphaBlendOp = vk::BlendOp::eMax,
		.colorWriteMask = vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA
	};

	///
	/// @brief Viewport state for fully-dynamic viewport & scissor
	///
	constexpr auto DYNAMIC_VIEWPORT_STATE = vk::PipelineViewportStateCreateInfo{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	///
	/// @brief Dynamic state array for fully-dynamic viewport & scissor
	///
	constexpr auto DYNAMIC_VIEWPORT_DYNSTATE = std::to_array({
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	});
}
