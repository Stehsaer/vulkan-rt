#pragma once

#include "vma.hpp"
#include "vulkan-context.hpp"

#include <vulkan/vulkan_raii.hpp>

struct Resources
{
	vma::Buffer vertex_buffer;
	vma::Image texture;
	vk::raii::ImageView texture_view;
	vk::raii::Sampler texture_sampler;

	vk::raii::DescriptorPool main_descriptor_pool;
	vk::raii::DescriptorSet main_descriptor_set;

	static std::expected<Resources, Error> create(
		const VulkanContext& context,
		vk::DescriptorSetLayout main_set_layout
	) noexcept;
};