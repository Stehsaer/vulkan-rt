#pragma once

#include "vulkan/platform/device.hpp"

namespace vulkan
{
	///
	/// @brief Get the headless test context
	///
	/// @return Reference to the headless test context
	///
	[[nodiscard]]
	const vulkan::HeadlessDeviceContext& get_test_context() noexcept;
}
