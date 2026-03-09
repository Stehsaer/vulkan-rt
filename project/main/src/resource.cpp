#include "resource.hpp"
#include "common/util/error.hpp"

namespace resource
{
	Layout Layout::create(const vulkan::DeviceContext& context)
	{
		auto camera_param_layout = interface::CameraParam::Layout::create(context->device)
			| Error::unwrap("Create camera param descriptor set layout failed");

		return Layout{.camera_param_layout = std::move(camera_param_layout)};
	}

	FrameResource FrameResource::create(const vulkan::DeviceContext& context, glm::u32vec2 swapchain_extent)
	{
		auto camera_param = interface::CameraParam::Resource::create(context->device, context->allocator)
			| Error::unwrap("Create camera param resource failed");

		auto depth_buffer =
			vulkan::FrameBuffer::create_depth(
				context->device,
				context->allocator,
				swapchain_extent,
				depth_format,
				depth_usages
			)
			| Error::unwrap("Create depth buffer failed");

		return FrameResource{
			.depth_buffer = std::move(depth_buffer),
			.camera_param = std::move(camera_param)
		};
	}

	void FrameDescriptorSet::bind_resource(
		const vk::raii::Device& device,
		const FrameResource& resource,
		const FrameResource& prev_frame_resource [[maybe_unused]]
	) const noexcept
	{
		camera_param.bind_resource(device, resource.camera_param);
	}

	DescriptorPool DescriptorPool::create(
		const vk::raii::Device& device,
		const Layout& layout,
		uint32_t set_count
	)
	{
		auto camera_param_pool =
			interface::CameraParam::DescriptorPool::create(device, layout.camera_param_layout, set_count)
			| Error::unwrap("Create camera param descriptor pool failed");

		return DescriptorPool(std::move(camera_param_pool));
	}

	std::vector<FrameDescriptorSet> DescriptorPool::get_frame_descriptor_sets() const
	{
		const auto camera_param_descriptor_sets = camera_param_pool.get_descriptor_sets();

		return std::views::zip(camera_param_descriptor_sets)
			| std::views::transform([](const auto& descriptor_set) {
				   const auto& [camera_param_set] = descriptor_set;
				   return FrameDescriptorSet{.camera_param = camera_param_set};
			   })
			| std::ranges::to<std::vector>();
	}

	SyncPrimitive SyncPrimitive::create(const vulkan::DeviceContext& context)
	{
		auto fence =
			context->device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled})
				.transform_error(Error::from<vk::Result>())
			| Error::unwrap("Create draw fence failed");

		auto render_finished_semaphore =
			context->device.createSemaphore({}).transform_error(Error::from<vk::Result>())
			| Error::unwrap("Create render finished semaphore failed");

		auto present_complete_semaphore =
			context->device.createSemaphore({}).transform_error(Error::from<vk::Result>())
			| Error::unwrap("Create present complete semaphore failed");

		return {
			.draw_fence = std::move(fence),
			.render_finished_semaphore = std::move(render_finished_semaphore),
			.image_available_semaphore = std::move(present_complete_semaphore)
		};
	}
}
