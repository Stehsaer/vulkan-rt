#pragma once

#include "common/util/error.hpp"
#include "vulkan/context/instance.hpp"

#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan::impl
{
	///
	/// @brief Initialize SDL library and vulkan libraries
	///
	/// @return `void` if success, or error
	///
	[[nodiscard]]
	std::expected<vk::raii::Context, Error> init_sdl() noexcept;

	///
	/// @brief Create a SDL window
	///
	/// @param config Window config
	/// @return Created SDL window handle, or error
	///
	[[nodiscard]]
	std::expected<SDL_Window*, Error> create_window(const WindowConfig& config) noexcept;

	///
	/// @brief Create a headless instance
	///
	/// @param context Vulkan context
	/// @param instance_config Instance config
	/// @return Created instance or error
	///
	[[nodiscard]]
	std::expected<vk::raii::Instance, Error> create_instance_headless(
		const vk::raii::Context& context,
		const InstanceConfig& instance_config
	) noexcept;

	///
	/// @brief Create a surface instance
	///
	/// @param context Vulkan context
	/// @param instance_config Instance config
	/// @param window_config Window config
	/// @return Created instance or error
	///
	[[nodiscard]]
	std::expected<vk::raii::Instance, Error> create_instance_surface(
		const vk::raii::Context& context,
		const InstanceConfig& instance_config,
		const WindowConfig& window_config
	) noexcept;
}
