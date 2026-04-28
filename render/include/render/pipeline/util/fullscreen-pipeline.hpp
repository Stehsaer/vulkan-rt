#pragma once

#include "common/util/error.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::fullscreen
{
	///
	/// @brief Input assembly state for fullscreen pipelines
	///
	constexpr auto INPUT_ASSEMBLY_STATE = vk::PipelineInputAssemblyStateCreateInfo{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	///
	/// @brief Rasterization state for fullscreen pipelines
	///
	constexpr auto RASTERIZATION_STATE = vk::PipelineRasterizationStateCreateInfo{
		.cullMode = vk::CullModeFlagBits::eNone,
		.lineWidth = 1.0,
	};

	///
	/// @brief Multisample state for fullscreen pipelines (disabled)
	///
	constexpr auto MULTISAMPLE_STATE =
		vk::PipelineMultisampleStateCreateInfo{.rasterizationSamples = vk::SampleCountFlagBits::e1};

	///
	/// @brief Depth/stencil state for fullscreen pipelines (disabled)
	///
	constexpr auto DEPTH_STENCIL_STATE = vk::PipelineDepthStencilStateCreateInfo{
		.depthTestEnable = vk::False,
		.depthWriteEnable = vk::False,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False
	};

	///
	/// @brief Color blend attachment state for fullscreen pipelines (disabled)
	///
	constexpr auto BLEND_STATE = vk::PipelineColorBlendAttachmentState{
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA
	};

	///
	/// @brief Vertex input state for fullscreen pipelines (no vertex attributes)
	///
	constexpr auto VERTEX_INPUT_STATE = vk::PipelineVertexInputStateCreateInfo{};

	///
	/// @brief Viewport state for fullscreen pipelines (dynamic viewport and scissor)
	///
	constexpr auto VIEWPORT_STATE = vk::PipelineViewportStateCreateInfo{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	///
	/// @brief Dynamic states used by fullscreen pipelines
	///
	constexpr auto DYNAMIC_STATES = std::to_array({
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	});

	///
	/// @brief Dynamic state info for fullscreen pipelines
	///
	constexpr auto DYNAMIC_STATE_INFO = vk::PipelineDynamicStateCreateInfo{
		.dynamicStateCount = static_cast<uint32_t>(DYNAMIC_STATES.size()),
		.pDynamicStates = DYNAMIC_STATES.data(),
	};

	///
	/// @brief Get the fullscreen triangle vertex shader module
	/// @returns Shader module
	///
	[[nodiscard]]
	std::expected<vk::raii::ShaderModule, Error> get_vertex_shader(const vk::raii::Device& device) noexcept;
}
