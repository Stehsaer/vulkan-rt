#pragma once

#include "common/utility/error.hpp"
#include "vulkan-context.hpp"
#include <expected>
#include <vulkan/vulkan_raii.hpp>
struct Pipeline
{
	vk::raii::PipelineLayout layout;
	vk::raii::Pipeline pipeline;

	static std::expected<Pipeline, Error> create(
		const VulkanContext& context,
		vk::DescriptorSetLayout main_set_layout
	) noexcept;
};