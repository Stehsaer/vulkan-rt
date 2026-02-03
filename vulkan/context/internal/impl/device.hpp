#pragma once

#include <SDL3/SDL_video.h>
#include <expected>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "vulkan/context.hpp"

namespace vulkan::context
{
	std::expected<vk::raii::PhysicalDevice, Error> pick_physical_device(
		const vk::raii::Instance& instance,
		const Features& features
	) noexcept;

	std::expected<std::tuple<vk::raii::Device, DeviceQueues>, Error> create_logical_device(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		const Features& features
	) noexcept;
}