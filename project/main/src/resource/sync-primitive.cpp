#include "resource/sync-primitive.hpp"

namespace resource
{
	std::expected<SyncPrimitive, Error> SyncPrimitive::create(const vulkan::DeviceContext& context) noexcept
	{
		auto fence_result =
			context.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled})
				.transform_error(Error::from<vk::Result>());
		auto render_finished_semaphore_result =
			context.device.createSemaphore({}).transform_error(Error::from<vk::Result>());
		auto present_complete_semaphore_result =
			context.device.createSemaphore({}).transform_error(Error::from<vk::Result>());

		if (!fence_result) return fence_result.error().forward("Create draw fence failed");
		if (!render_finished_semaphore_result)
			return render_finished_semaphore_result.error().forward(
				"Create render finished semaphore failed"
			);
		if (!present_complete_semaphore_result)
			return present_complete_semaphore_result.error().forward(
				"Create image available semaphore failed"
			);

		return SyncPrimitive{
			.draw_fence = std::move(*fence_result),
			.render_finished_semaphore = std::move(*render_finished_semaphore_result),
			.image_available_semaphore = std::move(*present_complete_semaphore_result)
		};
	}
}
