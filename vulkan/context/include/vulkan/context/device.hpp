#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc.hpp"
#include "vulkan/context/instance.hpp"
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Manages devices, queues and allocator
	/// @details
	/// - Call @p create to create a device context. Customize options by modifying the @p Config struct
	/// - Make sure to create a `InstanceContext` prior to creating this context
	///
	struct DeviceContext
	{
		struct Config
		{
			// placeholder
		};

		struct Queue
		{
			std::shared_ptr<vk::raii::Queue> queue;
			uint32_t family_index;
		};

		vk::raii::PhysicalDevice phy_device;
		vk::raii::Device device;
		vulkan::alloc::Allocator allocator;

		Queue graphics_queue;
		Queue compute_queue;
		Queue present_queue;

		///
		/// @brief Create a device context with the given configuration
		///
		/// @param context Instance context. Must be valid for the lifetime of the created device context
		/// @param config Configuration
		/// @return Device context or error
		///
		[[nodiscard]]
		static std::expected<DeviceContext, Error> create(
			const InstanceContext& context,
			const Config& config
		) noexcept;
	};
}