#pragma once

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
		const vk::raii::Queue& queue;  // Main queue, available for compute, transfer and graphics
		uint32_t family;               // Queue family of the main queue
	};
}
