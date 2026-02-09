#pragma once

#include "common/util/error.hpp"

#include <span>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan::util
{
	[[nodiscard]]
	std::expected<vk::raii::ShaderModule, Error> create_shader(
		const vk::raii::Device& device,
		std::span<const std::byte> span
	) noexcept;
}