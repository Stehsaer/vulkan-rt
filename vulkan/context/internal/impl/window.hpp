#pragma once

#include <SDL3/SDL_video.h>
#include <expected>
#include <glm/glm.hpp>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "vulkan/context.hpp"

namespace vulkan::context
{
	std::expected<std::unique_ptr<WindowWrapper>, Error> create_window(
		const WindowInfo& window_info
	) noexcept;

	std::expected<std::unique_ptr<SurfaceWrapper>, Error> create_surface(
		const vk::raii::Instance& instance,
		SDL_Window* window
	) noexcept;
}