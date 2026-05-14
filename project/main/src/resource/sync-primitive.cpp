#include "resource/sync-primitive.hpp"
#include "common/util/error.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <utility>
#include <vulkan/vulkan.hpp>

namespace resource
{
	std::expected<SyncPrimitive, Error> SyncPrimitive::create(const vulkan::Context& context) noexcept
	{
		auto fence_result = context.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
		auto render_finished_semaphore_result = context.device.createSemaphore({});
		auto present_complete_semaphore_result = context.device.createSemaphore({});

		if (!fence_result) return Error::from(fence_result);
		if (!render_finished_semaphore_result) return Error::from(render_finished_semaphore_result);
		if (!present_complete_semaphore_result) return Error::from(present_complete_semaphore_result);

		return SyncPrimitive{
			.draw_fence = std::move(*fence_result),
			.render_finished_semaphore = std::move(*render_finished_semaphore_result),
			.image_available_semaphore = std::move(*present_complete_semaphore_result)
		};
	}
}
