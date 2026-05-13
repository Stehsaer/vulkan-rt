#pragma once

#include "common/util/error.hpp"
#include "vulkan/context/device.hpp"
#include "vulkan/context/imgui.hpp"
#include "vulkan/context/instance.hpp"
#include "vulkan/context/swapchain.hpp"

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
