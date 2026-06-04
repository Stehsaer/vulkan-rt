#pragma once

#include "common/util/error.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace resource
{
	///
	/// @brief General sync primitives
	///
	struct FrameSyncPrimitive
	{
		// Signaled when rendering complete
		vk::raii::Fence draw_fence;

		// Signaled when last frame has finished present
		vk::raii::Semaphore image_available_semaphore;

		///
		/// @brief Create a set of  sync primitive
		///
		/// @param context Vulkan context
		/// @return Created sync primitive or error
		///
		[[nodiscard]]
		static std::expected<FrameSyncPrimitive, Error> create(const vulkan::Context& context) noexcept;
	};
}
