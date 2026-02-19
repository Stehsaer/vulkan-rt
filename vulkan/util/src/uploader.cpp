#include "vulkan/util/uploader.hpp"

namespace vulkan
{
	std::expected<void, Error> Uploader::upload_buffer(const BufferUploadParam& param) noexcept
	{
		auto staging_buffer_result = allocator.create_buffer(
			vk::BufferCreateInfo{
				.size = param.data.size(),
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vulkan::alloc::MemoryUsage::CpuToGpu
		);
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffer failed");
		auto staging_buffer = std::move(*staging_buffer_result);

		if (const auto result = staging_buffer.upload(param.data); !result)
			return result.error().forward("Upload data to staging buffer failed");

		buffer_upload_tasks.push_back(
			BufferUploadTask{
				.dst_buffer = param.dst_buffer,
				.staging_buffer = std::move(staging_buffer),
				.data_size = param.data.size()
			}
		);

		return {};
	}

	std::expected<void, Error> Uploader::upload_image(const ImageUploadParam& param) noexcept
	{
		auto staging_buffer_result = allocator.create_buffer(
			vk::BufferCreateInfo{
				.size = param.data.size(),
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vulkan::alloc::MemoryUsage::CpuToGpu
		);
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffer failed");
		auto staging_buffer = std::move(*staging_buffer_result);

		if (const auto result = staging_buffer.upload(param.data); !result)
			return result.error().forward("Upload data to staging buffer failed");

		image_upload_tasks.push_back(
			ImageUploadTask{
				.dst_image = param.dst_image,
				.staging_buffer = std::move(staging_buffer),
				.buffer_row_length = param.buffer_row_length,
				.buffer_image_height = param.buffer_image_height,
				.subresource_layers = param.subresource_layers,
				.image_extent = param.image_extent,
				.dst_layout = param.dst_layout
			}
		);

		return {};
	}

	std::expected<void, Error> Uploader::execute() noexcept
	{
		auto command_pool_result =
			device
				.createCommandPool(
					{.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = queue_family}
				)
				.transform_error(Error::from<vk::Result>());
		if (!command_pool_result) return command_pool_result.error().forward("Create command pool failed");
		auto command_pool = std::move(*command_pool_result);

		auto allocated_command_buffers_result =
			device
				.allocateCommandBuffers(
					{.commandPool = command_pool,
					 .level = vk::CommandBufferLevel::ePrimary,
					 .commandBufferCount = 1}
				)
				.transform_error(Error::from<vk::Result>());
		if (!allocated_command_buffers_result)
			return allocated_command_buffers_result.error().forward("Allocate command buffer failed");
		auto allocated_command_buffers = std::move(*allocated_command_buffers_result);
		auto command_buffer = std::move(allocated_command_buffers[0]);

		auto fence_result = device.createFence({}).transform_error(Error::from<vk::Result>());
		if (!fence_result) return fence_result.error().forward("Create fence failed");
		auto fence = std::move(*fence_result);

		const auto buffer_barriers_after_copying =
			buffer_upload_tasks
			| std::views::transform([](const auto& task) {
				  return vk::BufferMemoryBarrier2{
					  .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					  .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					  .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
					  .dstAccessMask = vk::AccessFlagBits2::eNone,
					  .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					  .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					  .buffer = task.dst_buffer,
					  .offset = 0,
					  .size = VK_WHOLE_SIZE,
				  };
			  })
			| std::ranges::to<std::vector>();

		const auto barrier_before_transition =
			image_upload_tasks
			| std::views::transform([](const auto& task) {
				  return vk::ImageMemoryBarrier2{
					  .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
					  .srcAccessMask = {},
					  .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
					  .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
					  .oldLayout = vk::ImageLayout::eUndefined,
					  .newLayout = vk::ImageLayout::eTransferDstOptimal,
					  .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					  .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					  .image = task.dst_image,
					  .subresourceRange = {
										.aspectMask = task.subresource_layers.aspectMask,
										.baseMipLevel = task.subresource_layers.mipLevel,
										.levelCount = 1,
										.baseArrayLayer = task.subresource_layers.baseArrayLayer,
										.layerCount = task.subresource_layers.layerCount,
										},
				  };
			  })
			| std::ranges::to<std::vector>();

		const auto barrier_after_transition =
			image_upload_tasks
			| std::views::transform([](const ImageUploadTask& task) {
				  return vk::ImageMemoryBarrier2{
					  .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					  .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					  .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
					  .dstAccessMask = vk::AccessFlagBits2::eNone,
					  .oldLayout = vk::ImageLayout::eTransferDstOptimal,
					  .newLayout = task.dst_layout,
					  .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					  .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					  .image = task.dst_image,
					  .subresourceRange = {
										   .aspectMask = task.subresource_layers.aspectMask,
										   .baseMipLevel = task.subresource_layers.mipLevel,
										   .levelCount = 1,
										   .baseArrayLayer = task.subresource_layers.baseArrayLayer,
										   .layerCount = task.subresource_layers.layerCount,
										   },
				  };
			  })
			| std::ranges::to<std::vector>();

		command_buffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
		{

			for (const auto& task : buffer_upload_tasks)
			{
				const auto copy_region = vk::BufferCopy{
					.srcOffset = 0,
					.dstOffset = 0,
					.size = task.data_size,
				};
				command_buffer.copyBuffer(task.staging_buffer, task.dst_buffer, copy_region);
			}

			command_buffer.pipelineBarrier2(
				vk::DependencyInfo{}
					.setBufferMemoryBarriers(buffer_barriers_after_copying)
					.setImageMemoryBarriers(barrier_before_transition)
			);

			for (const auto& task : image_upload_tasks)
			{
				const auto buffer_image_copy = vk::BufferImageCopy{
					.bufferOffset = 0,
					.bufferRowLength = uint32_t(task.buffer_row_length),
					.bufferImageHeight = uint32_t(task.buffer_image_height),
					.imageSubresource = task.subresource_layers,
					.imageOffset = {.x = 0, .y = 0, .z = 0},
					.imageExtent = task.image_extent,
				};
				command_buffer.copyBufferToImage(
					task.staging_buffer,
					task.dst_image,
					vk::ImageLayout::eTransferDstOptimal,
					buffer_image_copy
				);
			}

			command_buffer.pipelineBarrier2(
				vk::DependencyInfo{}.setImageMemoryBarriers(barrier_after_transition)
			);
		}
		command_buffer.end();

		const auto command_buffers = std::to_array<vk::CommandBuffer>({command_buffer});
		const auto submit_info = vk::SubmitInfo{}.setCommandBuffers(command_buffers);
		transfer_queue.submit(submit_info, fence);

		if (const auto wait_fence_result =
				device.waitForFences({fence}, vk::True, std::numeric_limits<uint64_t>::max());
			wait_fence_result != vk::Result::eSuccess)
			return Error("Wait for upload fence failed", vk::to_string(wait_fence_result));

		buffer_upload_tasks.clear();
		image_upload_tasks.clear();

		return {};
	}
}
