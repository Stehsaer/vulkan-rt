#pragma once

#include "vulkan/interface/common-context.hpp"

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace resource
{
	///
	/// @brief General sync primitives
	///
	struct SyncPrimitive
	{
		// Signaled when rendering complete
		vk::raii::Fence draw_fence;

		// Signaled when rendering complete (same as `draw_fence`)
		vk::raii::Semaphore render_finished_semaphore;

		// Signaled when last frame has finished present
		vk::raii::Semaphore image_available_semaphore;

		///
		/// @brief Create a set of  sync primitive
		///
		/// @param context Vulkan context
		/// @return Created sync primitive or error
		///
		[[nodiscard]]
		static std::expected<SyncPrimitive, Error> create(const vulkan::DeviceContext& context) noexcept;
	};
}
