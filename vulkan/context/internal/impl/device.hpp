#pragma once

#include "vulkan/context/device.hpp"
#include "vulkan/context/instance.hpp"
#include "vulkan/util/linked-struct.hpp"

#include <vulkan/vulkan_raii.hpp>

namespace vulkan::impl
{
	///
	/// @brief Stores the result of a successful check (headless device)
	///
	///
	struct HeadlessDeviceInfo
	{
		vk::raii::PhysicalDevice phy_device;
		vulkan::LinkedStruct<vk::PhysicalDeviceFeatures2> features;
		std::vector<std::string> extensions;
		uint32_t render_family_index;
		float rank;

		///
		/// @brief Create a headless device and a render queue
		///
		/// @return `(Device, Render Queue)` or error
		///
		[[nodiscard]]
		std::expected<std::pair<vk::raii::Device, DeviceQueue>, Error> create_device() const noexcept;
	};

	///
	/// @brief Stores the result of a successful check (surface device)
	///
	///
	struct SurfaceDeviceInfo
	{
		vk::raii::PhysicalDevice phy_device;
		vulkan::LinkedStruct<vk::PhysicalDeviceFeatures2> features;
		std::vector<std::string> extensions;
		uint32_t render_family_index;
		uint32_t present_family_index;
		float rank;

		///
		/// @brief Create a surface device and render+present queue
		///
		/// @return `(Device, Render Queue, Present Queue)` or error
		///
		[[nodiscard]]
		std::expected<
			std::tuple<vk::raii::Device, DeviceQueue, DeviceQueue>,
			Error
		> create_device() const noexcept;
	};

	///
	/// @brief Stores fail information for a physical device
	///
	///
	struct FailInfo
	{
		vk::raii::PhysicalDevice phy_device;
		Error error;
	};

	///
	/// @brief Check and acquire info for a headless device
	///
	/// @param phy_device Physical device
	/// @param config Device config
	/// @retval HeadlessDeviceInfo Success, all info required for creating a device
	/// @retval FailInfo Fail, error info explaining the cause
	///
	[[nodiscard]]
	std::variant<HeadlessDeviceInfo, FailInfo> check_headless_device(
		const vk::raii::PhysicalDevice& phy_device,
		const DeviceConfig& config
	) noexcept;

	///
	/// @brief Check and acquire info for a surface device
	///
	/// @param phy_device Physical device
	/// @param instance Surface instance
	/// @param config Device config
	/// @retval SurfaceDeviceInfo Success, all info required for creatin ga device
	/// @retval FailInfo Fail, error info explaining the cause
	///
	[[nodiscard]]
	std::variant<SurfaceDeviceInfo, FailInfo> check_surface_device(
		const vk::raii::PhysicalDevice& phy_device,
		const SurfaceInstanceContext& instance,
		const DeviceConfig& config
	) noexcept;
}
