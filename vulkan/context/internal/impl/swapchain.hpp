#pragma once

#include <SDL3/SDL_video.h>
#include <expected>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "vulkan/context.hpp"

namespace vulkan::context
{
	std::expected<SwapchainLayout, Error> select_swapchain_layout(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		const DeviceQueues& queues
	) noexcept;
}