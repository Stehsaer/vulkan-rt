#pragma once

#include "common/util/error.hpp"

#include <span>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan::util
{
	///
	/// @brief Creates a shader module
	///
	/// @param span SPIR-V binary span
	/// @return Created shader module or Error
	///
	[[nodiscard]]
	std::expected<vk::raii::ShaderModule, Error> create_shader(
		const vk::raii::Device& device,
		std::span<const std::byte> span
	) noexcept;
}