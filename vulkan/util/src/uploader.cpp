#include "vulkan/util/uploader.hpp"

namespace vulkan::util
{
	std::expected<void, Error> Uploader::upload_buffer(const BufferUploadParam& param) noexcept
	{
		auto staging_buffer_expected = allocator.create_buffer(
			vk::BufferCreateInfo{
				.size = param.data.size(),
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vulkan::alloc::MemoryUsage::CpuToGpu
		);
		if (!staging_buffer_expected)
			return staging_buffer_expected.error().forward("Create staging buffer failed");

		if (const auto map_result = staging_buffer_expected->upload(param.data); !map_result)
			return map_result.error().forward("Upload data to staging buffer failed");

		buffer_upload_tasks.push_back(
			BufferUploadTask{
				.dst_buffer = param.dst_buffer,
				.staging_buffer = std::move(*staging_buffer_expected),
				.data_size = param.data.size()
			}
		);

		return {};
	}

	std::expected<void, Error> Uploader::upload_image(const ImageUploadParam& param) noexcept
	{
		auto staging_buffer_expected = allocator.create_buffer(
			vk::BufferCreateInfo{
				.size = param.data.size(),
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vulkan::alloc::MemoryUsage::CpuToGpu
		);
		if (!staging_buffer_expected)
			return staging_buffer_expected.error().forward("Create staging buffer failed");

		if (const auto map_result = staging_buffer_expected->upload(param.data); !map_result)
			return map_result.error().forward("Upload data to staging buffer failed");

		image_upload_tasks.push_back(
			ImageUploadTask{
				.dst_image = param.dst_image,
				.staging_buffer = std::move(*staging_buffer_expected),
				.buffer_row_length = param.buffer_row_length,
				.buffer_image_height = param.buffer_image_height,
				.subresource_layers = param.subresource_layers,
				.image_extent = param.image_extent,
				.dst_layout = param.dst_layout
			}
		);

		return {};
	}

	std::expected<void, Error> Uploader::execute() const noexcept
	{
		auto command_pool_expected = device.createCommandPool(
			{.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = queue_family}
		);
		if (!command_pool_expected) return Error(command_pool_expected.error(), "Create command pool failed");
		auto command_pool = std::move(*command_pool_expected);

		auto command_buffer_expected = device.allocateCommandBuffers(
			{.commandPool = command_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1}
		);
		if (!command_buffer_expected)
			return Error(command_buffer_expected.error(), "Allocate command buffer failed");
		auto command_buffer = std::move((*command_buffer_expected)[0]);

		auto fence_expected = device.createFence({});
		if (!fence_expected) return Error(fence_expected.error(), "Create upload fence failed");
		auto fence = std::move(*fence_expected);

		const auto buffer_barriers_after_copying =
			buffer_upload_tasks
			| std::views::transform([](const auto& task) {
				  return vk::BufferMemoryBarrier2{
					  .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					  .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					  .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
					  .dstAccessMask = vk::AccessFlagBits2::eNone,
					  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
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
					  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
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
					  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
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
			return Error(wait_fence_result, "Wait for upload fence failed");

		return {};
	}
}