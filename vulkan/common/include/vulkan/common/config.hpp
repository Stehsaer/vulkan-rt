#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>

namespace vulkan::config
{
	///
	/// @brief Current build profile (debug/release)
	///
#ifdef NDEBUG
	static constexpr bool DEBUG = false;
#else
	static constexpr bool DEBUG = true;
#endif

	///
	/// @brief Default setup for validation layer. Enabled in debug profile.
	///
	static constexpr bool DEFAULT_VALIDATION_ENABLED = DEBUG;

	///
	/// @brief Default setup for robust buffer access. Enabled in debug profile
	///
	static constexpr bool DEFAULT_ROBUST_BUFFER_ACCESS_ENABLED = DEBUG;

	///
	/// @brief Default API version. Currently vk 1.4
	///
	static constexpr uint32_t API_VERSION = vk::ApiVersion14;
}
