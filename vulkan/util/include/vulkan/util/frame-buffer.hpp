#pragma once

#include "vulkan/alloc.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	struct FrameBuffer
	{
		vulkan::alloc::Image image;
		vk::raii::ImageView view;

		static std::expected<FrameBuffer, Error> create_color(
			const vk::raii::Device& device,
			const vulkan::alloc::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;

		static std::expected<FrameBuffer, Error> create_depth(
			const vk::raii::Device& device,
			const vulkan::alloc::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;

		static std::expected<FrameBuffer, Error> create_depth_stencil(
			const vk::raii::Device& device,
			const vulkan::alloc::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;
	};
}
