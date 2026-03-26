#pragma once

#include <mutex>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/alloc.hpp"

namespace vulkan
{
	///
	/// @brief Primary device context for rendering and transferring
	///
	struct DeviceContext
	{
		const vk::raii::PhysicalDevice& phy_device;
		const vk::raii::Device& device;
		const vulkan::Allocator& allocator;

		// Main queue, available for compute, transfer and graphics
		const vk::raii::Queue& queue;

		// Mutex for synchronizing queue submissions, use it only when multi-threading
		std::mutex& submit_mutex;

		// Queue family of the main queue
		uint32_t family;
	};
}
