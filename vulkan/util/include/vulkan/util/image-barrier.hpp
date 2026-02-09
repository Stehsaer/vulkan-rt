#pragma once

#include <vulkan/vulkan.hpp>

namespace vulkan::util::image_barrier
{
	///
	/// @brief Create an image barrier for acquiring a swapchain image
	///
	/// @param image Swapchain image
	/// @return Image memory barrier for acquiring the image
	///
	[[nodiscard]]
	vk::ImageMemoryBarrier2 swapchain_acquire(vk::Image image) noexcept;

	///
	/// @brief Create an image barrier for presenting a swapchain image
	///
	/// @param image Swapchain image
	/// @return Image memory barrier for presenting the image
	///
	[[nodiscard]]
	vk::ImageMemoryBarrier2 swapchain_present(vk::Image image) noexcept;
}