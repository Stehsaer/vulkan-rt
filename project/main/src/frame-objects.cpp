#include "frame-objects.hpp"
#include "vulkan/util/frame-buffer.hpp"

FrameSyncPrimitive FrameSyncPrimitive::create(const vulkan::DeviceContext& context)
{
	auto fence =
		context.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled})
			.transform_error(Error::from<vk::Result>())
		| Error::unwrap("Create draw fence failed");

	auto render_finished_semaphore =
		context.device.createSemaphore({}).transform_error(Error::from<vk::Result>())
		| Error::unwrap("Create render finished semaphore failed");

	auto present_complete_semaphore =
		context.device.createSemaphore({}).transform_error(Error::from<vk::Result>())
		| Error::unwrap("Create present complete semaphore failed");

	return {
		.draw_fence = std::move(fence),
		.render_finished_semaphore = std::move(render_finished_semaphore),
		.image_available_semaphore = std::move(present_complete_semaphore)
	};
}

FrameRenderResource FrameRenderResource::create(
	const vulkan::DeviceContext& context,
	glm::u32vec2 swapchain_extent
)
{
	auto depth_buffer =
		vulkan::FrameBuffer::create_depth(
			context.device,
			context.allocator,
			swapchain_extent,
			depth_format,
			depth_usages
		)
		| Error::unwrap("Create depth buffer failed");

	return {.depth_buffer = std::move(depth_buffer)};
}
