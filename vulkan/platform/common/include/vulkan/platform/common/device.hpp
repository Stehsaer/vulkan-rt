#pragma once

#include "common/container/error.hpp"
#include "common/container/linked-struct.hpp"
#include "vulkan/common/context.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan::platform
{
	///
	/// @brief Check for device constraints given specific feature
	///
	/// @param phy_device Physical device
	/// @param feature Device feature
	/// @return `void` if pass, `Error` if failed
	///
	[[nodiscard]]
	std::expected<void, Error> check_device_constraints(
		const vk::raii::PhysicalDevice& phy_device,
		const DeviceFeature& feature [[maybe_unused]]
	) noexcept;

	///
	/// @brief Find device features given specific feature
	///
	/// @param phy_device Physical device
	/// @param feature Device feature
	/// @return Feature linked structs if success, `Error` if failed
	///
	[[nodiscard]]
	std::expected<util::LinkedStruct<vk::PhysicalDeviceFeatures2>, Error> find_device_features(
		const vk::raii::PhysicalDevice& phy_device,
		const DeviceFeature& feature
	) noexcept;

	///
	/// @brief Find device extensions given specific feature
	///
	/// @param phy_device Physical device
	/// @param feature Device feature
	/// @param support_present Whether to enable presenting (swapchain) support
	/// @return Array of extension names, or error if not supported
	///
	[[nodiscard]]
	std::expected<std::vector<std::string>, Error> find_device_extensions(
		const vk::raii::PhysicalDevice& phy_device,
		const DeviceFeature& feature [[maybe_unused]],
		bool support_present
	) noexcept;

	///
	/// @brief Look for render queue family
	///
	/// @param phy_device Physical device
	/// @return Render queue family, or error if none found
	///
	[[nodiscard]]
	std::expected<uint32_t, Error> find_render_queue(const vk::raii::PhysicalDevice& phy_device) noexcept;

	///
	/// @brief Look for present queue family
	///
	/// @param phy_device Physical device
	/// @param surface Vulkan surface
	/// @param render_queue Render queue family (use `find_render_queue` to obtain)
	/// @return Present queue family, or error if none found
	///
	[[nodiscard]]
	std::expected<uint32_t, Error> find_present_queue(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		uint32_t render_queue
	) noexcept;

	///
	/// @brief Rank the device by multiple factors. Prefers discrete GPU with largest VRAM
	///
	/// @param phy_device Physical device
	/// @return Device rank represented in floating-point number
	///
	[[nodiscard]]
	float rank_device(const vk::raii::PhysicalDevice& phy_device) noexcept;
}
