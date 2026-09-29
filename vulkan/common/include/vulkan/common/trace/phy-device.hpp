#pragma once

#include "common/container/debug-trace.hpp"
#include "common/container/error.hpp"

#include <expected>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Generate a trace for physical device
	///
	/// @param phy_device Vulkan physical device
	/// @return Trace
	///
	[[nodiscard]]
	debug::Trace trace_phy_device(const vk::raii::PhysicalDevice& phy_device) noexcept;
}
