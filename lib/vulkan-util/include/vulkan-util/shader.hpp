#pragma once

#include "common/utility/error.hpp"

#include <span>
#include <vulkan/vulkan_raii.hpp>

namespace vkutil
{
	std::expected<vk::raii::ShaderModule, Error> create_shader(
		const vk::raii::Device& device,
		std::span<const std::byte> span
	) noexcept;
}