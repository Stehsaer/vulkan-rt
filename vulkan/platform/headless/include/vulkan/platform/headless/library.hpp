#pragma once

#include "common/container/error.hpp"

#include <expected>
#include <memory>
#include <vulkan/vulkan_core.h>

namespace vulkan
{
	///
	/// @brief Vulkan library handle. Opaque implementation varies across platforms.
	///
	using VulkanLibrary = std::unique_ptr<struct VulkanLibraryInfo, void (*)(VulkanLibraryInfo*)>;

	///
	/// @brief Load vulkan library
	///
	/// @return Loaded vulkan library instance or error
	///
	[[nodiscard]]
	std::expected<VulkanLibrary, Error> load_vulkan_library() noexcept;

	///
	/// @brief Get the vulkan `vkGetInstanceProcAddr` from the library
	///
	/// @param info Vulkan library information
	/// @return Function pointer or error
	///
	[[nodiscard]]
	std::expected<PFN_vkGetInstanceProcAddr, Error> get_vulkan_dyn_func(
		const VulkanLibraryInfo& info
	) noexcept;
}
