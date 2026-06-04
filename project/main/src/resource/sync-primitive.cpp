#include "resource/sync-primitive.hpp"
#include "common/util/error.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <utility>
#include <vulkan/vulkan.hpp>

namespace resource
{
	std::expected<FrameSyncPrimitive, Error> FrameSyncPrimitive::create(
		const vulkan::Context& context
	) noexcept
	{
		auto fence_result = context.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
		auto image_available_semaphore = context.device.createSemaphore({});

		if (!fence_result) return Error::from(fence_result);
		if (!image_available_semaphore) return Error::from(image_available_semaphore);

		return FrameSyncPrimitive{
			.draw_fence = std::move(*fence_result),
			.image_available_semaphore = std::move(*image_available_semaphore)
		};
	}
}
