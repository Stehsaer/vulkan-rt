#pragma once

#include "common/container/error.hpp"
#include "vulkan/platform/device.hpp"
#include "vulkan/platform/imgui.hpp"
#include "vulkan/platform/instance.hpp"
#include "vulkan/platform/swapchain.hpp"

#include <expected>

namespace resource
{
	///
	/// @brief Vulkan-related contexts
	///
	struct Context
	{
		vulkan::SurfaceInstanceContext instance;
		vulkan::SurfaceDeviceContext device;
		vulkan::SwapchainContext swapchain;
		vulkan::ImGuiContext imgui;

		///
		/// @brief Create vulkan contexts
		///
		/// @return Created context or error
		///
		[[nodiscard]]
		static std::expected<Context, Error> create() noexcept;
	};
}
