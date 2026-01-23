#pragma once

#include "common/utility/error.hpp"

#include <vulkan/vulkan_raii.hpp>

struct DescriptorLayouts
{
	vk::raii::DescriptorSetLayout main_layout;

	static std::expected<DescriptorLayouts, Error> create(const vk::raii::Device& device) noexcept;
};