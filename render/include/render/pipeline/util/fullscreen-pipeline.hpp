#pragma once

#include "common/util/error.hpp"
#include "render/pipeline/util/constant.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::fullscreen
{
	///
	/// @brief Vertex input state for fullscreen pipelines (no vertex attributes)
	///
	constexpr auto VERTEX_INPUT_STATE = vk::PipelineVertexInputStateCreateInfo{};

	///
	/// @brief Dynamic state info for fullscreen pipelines
	///
	constexpr auto DYNAMIC_STATE_INFO = vk::PipelineDynamicStateCreateInfo{
		.dynamicStateCount = static_cast<uint32_t>(constant::DYNAMIC_VIEWPORT_DYNSTATE.size()),
		.pDynamicStates = constant::DYNAMIC_VIEWPORT_DYNSTATE.data(),
	};

	///
	/// @brief Get the fullscreen triangle vertex shader module
	/// @returns Shader module
	///
	[[nodiscard]]
	std::expected<vk::raii::ShaderModule, Error> get_vertex_shader(const vk::raii::Device& device) noexcept;
}
