#pragma once

#include "vulkan/platform/headless/device.hpp"

namespace vulkan
{
	///
	/// @brief Get the headless test context
	///
	/// @return Reference to the headless test context
	///
	[[nodiscard]]
	const vulkan::HeadlessDevice& get_test_context() noexcept;
}
