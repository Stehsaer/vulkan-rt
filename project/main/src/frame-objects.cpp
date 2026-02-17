#include "frame-objects.hpp"
#include "vulkan/util/constants.hpp"

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
	const auto depth_buffer_create_info = vk::ImageCreateInfo{
		.imageType = vk::ImageType::e2D,
		.format = vk::Format::eD32Sfloat,
		.extent = {.width = swapchain_extent.x, .height = swapchain_extent.y, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment
	};
	auto depth_buffer =
		context.allocator.create_image(depth_buffer_create_info, vulkan::alloc::MemoryUsage::GpuOnly)
		| Error::unwrap("Create depth buffer failed");

	const auto depth_buffer_view_create_info = vk::ImageViewCreateInfo{
		.image = depth_buffer,
		.viewType = vk::ImageViewType::e2D,
		.format = depth_buffer_create_info.format,
		.subresourceRange = vulkan::base_level_image(vk::ImageAspectFlagBits::eDepth)
	};
	auto depth_buffer_view =
		context.device.createImageView(depth_buffer_view_create_info)
			.transform_error(Error::from<vk::Result>())
		| Error::unwrap("Create depth buffer view failed");

	return {.depth_buffer = std::move(depth_buffer), .depth_buffer_view = std::move(depth_buffer_view)};
}
