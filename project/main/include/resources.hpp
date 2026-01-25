#pragma once

#include "vulkan/alloc.hpp"
#include "vulkan/context.hpp"

#include <vulkan/vulkan_raii.hpp>

struct Resources
{
	vulkan::alloc::Buffer vertex_buffer;
	vulkan::alloc::Image texture;
	vk::raii::ImageView texture_view;
	vk::raii::Sampler texture_sampler;

	vk::raii::DescriptorPool main_descriptor_pool;
	vk::raii::DescriptorSet main_descriptor_set;

	static std::expected<Resources, Error> create(
		const vulkan::Context& context,
		vk::DescriptorSetLayout main_set_layout
	) noexcept;
};