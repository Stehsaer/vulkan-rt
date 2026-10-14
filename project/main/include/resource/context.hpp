#pragma once

#include "common/container/error.hpp"
#include "vulkan/platform/windowed/device.hpp"
#include "vulkan/platform/windowed/imgui.hpp"
#include "vulkan/platform/windowed/instance.hpp"
#include "vulkan/platform/windowed/swapchain.hpp"

#include <expected>

namespace resource
{
	///
	/// @brief Vulkan-related contexts
	///
	struct Context
	{
		vulkan::WindowedInstance instance;
		vulkan::WindowedDevice device;
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
