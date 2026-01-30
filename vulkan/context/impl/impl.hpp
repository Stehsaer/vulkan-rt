#pragma once

#include <SDL3/SDL_video.h>
#include <cstdint>
#include <expected>
#include <glm/glm.hpp>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "vulkan/context/vulkan.hpp"
#include "vulkan/context/window.hpp"

namespace vulkan::context::impl
{
	static constexpr uint32_t api_version = vk::ApiVersion14;

	std::expected<std::unique_ptr<WindowWrapper>, Error> create_window(
		const WindowInfo& window_info
	) noexcept;

	std::expected<std::span<const char* const>, Error> get_instance_extensions() noexcept;

	std::expected<vk::raii::Instance, Error> create_instance(
		const vk::raii::Context& context,
		const AppInfo& app_info,
		const Features& features
	) noexcept;

	std::expected<std::unique_ptr<SurfaceWrapper>, Error> create_surface(
		const vk::raii::Instance& instance,
		SDL_Window* window
	) noexcept;

	std::expected<vk::raii::PhysicalDevice, Error> pick_physical_device(
		const vk::raii::Instance& instance,
		const Features& features
	) noexcept;

	std::expected<std::tuple<vk::raii::Device, DeviceQueues>, Error> create_logical_device(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		const Features& features
	) noexcept;

	std::expected<SwapchainLayout, Error> select_swapchain_layout(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		const DeviceQueues& queues
	) noexcept;
}