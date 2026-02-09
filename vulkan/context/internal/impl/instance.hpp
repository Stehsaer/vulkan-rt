#pragma once

#include <SDL3/SDL_video.h>
#include <expected>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "vulkan/context.hpp"

namespace vulkan::context
{

	std::expected<vk::raii::Instance, Error> create_instance(
		const vk::raii::Context& context,
		const AppInfo& app_info,
		const Features& features
	) noexcept;

}
