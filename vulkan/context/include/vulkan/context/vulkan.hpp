#pragma once

#include <SDL3/SDL_video.h>
#include <cstdint>
#include <expected>
#include <glm/glm.hpp>
#include <memory>
#include <set>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "vulkan/util/linked-struct.hpp"

namespace vulkan
{
	///
	/// @brief Containing basic application info for Vulkan instance creation
	///
	///
	struct AppInfo
	{
		std::string application_name = "Vulkan Application";
		uint32_t application_version = VK_MAKE_VERSION(0, 0, 0);
		std::string engine_name = "No Engine";
		uint32_t engine_version = VK_MAKE_VERSION(0, 0, 0);
	};

	///
	/// @brief Struct representing requested Vulkan features, set to `true` to enable corresponding features
	///
	///
	struct Features
	{
		bool validation = true;
		bool ray_tracing = false;  // Placeholder for future feature

		///
		/// @brief Get instance layers required by the features
		///
		/// @return Set of instance layer names
		///
		std::set<std::string> get_instance_layers() const noexcept;

		///
		/// @brief Get instance extensions required by the features
		///
		/// @return Set of instance extension names
		///
		std::set<std::string> get_instance_extensions() const noexcept;

		///
		/// @brief Check if the physical device supports the required features
		///
		/// @param phy_device Physical device to check
		/// @return True if the device supports the features, false otherwise
		///
		bool check_device(const vk::raii::PhysicalDevice& phy_device) const noexcept;

		///
		/// @brief Get device extensions required by the features
		///
		/// @return Set of device extension names
		///
		std::set<std::string> get_device_extensions() const noexcept;

		///
		/// @brief Get the device features chain for device creation
		///
		/// @return Feature chain
		///
		vulkan::util::LinkedStruct<vk::PhysicalDeviceFeatures2> get_device_features_chain() const noexcept;

	  private:

		// Get the base device features for Vulkan 1.0
		static vk::PhysicalDeviceFeatures required_vulkan10_features() noexcept;

		// Check if the required device features are supported
		static bool check_vulkan10_feature(
			const vk::PhysicalDeviceFeatures& required,
			const vk::PhysicalDeviceFeatures& available
		) noexcept;

		static constexpr auto mandatory_device_extensions =
			std::to_array({vk::KHRSwapchainExtensionName, vk::KHRSynchronization2ExtensionName});
	};

	///
	/// @brief Stores queue families and corresponding queues used by the device
	///
	///
	struct DeviceQueues
	{
		std::shared_ptr<vk::raii::Queue> graphics;
		std::shared_ptr<vk::raii::Queue> compute;
		std::shared_ptr<vk::raii::Queue> present;

		uint32_t graphics_index;
		uint32_t compute_index;
		uint32_t present_index;
	};

	///
	/// @brief Stores selected swapchain layout details
	///
	///
	struct SwapchainLayout
	{
		vk::SharingMode image_sharing_mode;
		std::vector<uint32_t> image_queue_family_indices;
		vk::SurfaceFormatKHR surface_format;
		vk::PresentModeKHR present_mode;
	};

	///
	/// @brief Wrapper for automatically destroying a Vulkan surface
	///
	///
	class SurfaceWrapper
	{
	  public:

		SurfaceWrapper(vk::Instance instance, vk::SurfaceKHR surface) :
			instance(instance),
			surface(surface)
		{}

		~SurfaceWrapper() noexcept;

		operator vk::SurfaceKHR() const noexcept { return surface; }

	  private:

		vk::Instance instance;
		vk::SurfaceKHR surface;
	};
}