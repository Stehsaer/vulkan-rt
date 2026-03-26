#include "vulkan/util/resource-readback.hpp"
#include "vulkan/alloc.hpp"
#include "vulkan/util/constants.hpp"

#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan::impl
{
	namespace
	{
		struct CommitResource
		{
			vk::raii::CommandPool command_pool;
			vk::raii::CommandBuffer command_buffer;
			vk::raii::Fence fence;

			static std::expected<CommitResource, Error> create(
				const vk::raii::Device& device,
				uint32_t queue_family
			) noexcept
			{
				auto command_pool_result =
					device
						.createCommandPool(
							vk::CommandPoolCreateInfo{
								.flags = vk::CommandPoolCreateFlagBits::eTransient,
								.queueFamilyIndex = queue_family,
							}
						)
						.transform_error(Error::from<vk::Result>());
				if (!command_pool_result)
					return command_pool_result.error().forward("Create command pool failed");
				auto command_pool = std::move(*command_pool_result);

				auto command_buffer_result =
					device
						.allocateCommandBuffers(
							vk::CommandBufferAllocateInfo{
								.commandPool = command_pool,
								.level = vk::CommandBufferLevel::ePrimary,
								.commandBufferCount = 1
							}
						)
						.transform_error(Error::from<vk::Result>())
						.transform([](std::vector<vk::raii::CommandBuffer> list) {
							return std::move(list.front());
						});

				if (!command_buffer_result)
					return command_buffer_result.error().forward("Allocate command buffer failed");
				auto command_buffer = std::move(*command_buffer_result);

				auto fence_result =
					device.createFence(vk::FenceCreateInfo{}).transform_error(Error::from<vk::Result>());
				if (!fence_result) return fence_result.error().forward("Create fence failed");
				auto fence = std::move(*fence_result);

				return CommitResource{
					.command_pool = std::move(command_pool),
					.command_buffer = std::move(command_buffer),
					.fence = std::move(fence)
				};
			}
		};

		struct TransferResource
		{
			vulkan::alloc::Image blit_dst_image;
			vulkan::alloc::Buffer readback_buffer;

			static std::expected<TransferResource, Error> create(
				const vulkan::Allocator& allocator,
				glm::u32vec2 image_size,
				size_t buffer_size,
				vk::Format format
			) noexcept
			{
				const auto image_create_info = vk::ImageCreateInfo{
					.imageType = vk::ImageType::e2D,
					.format = format,
					.extent = {.width = image_size.x, .height = image_size.y, .depth = 1},
					.mipLevels = 1,
					.arrayLayers = 1,
					.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc
				};
				auto blit_dst_image_result =
					allocator.create_image(image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
				if (!blit_dst_image_result)
					return blit_dst_image_result.error().forward("Create blit destination image failed");

				const auto buffer_create_info = vk::BufferCreateInfo{
					.size = buffer_size,
					.usage = vk::BufferUsageFlagBits::eTransferDst,
				};
				auto readback_buffer_result =
					allocator.create_buffer(buffer_create_info, vulkan::alloc::MemoryUsage::GpuToCpu);
				if (!readback_buffer_result)
					return readback_buffer_result.error().forward("Create readback buffer failed");

				return TransferResource{
					.blit_dst_image = std::move(*blit_dst_image_result),
					.readback_buffer = std::move(*readback_buffer_result)
				};
			}
		};
	}

	std::expected<void, Error> readback_image(
		const vulkan::DeviceContext& context,
		vk::Image src_image,
		vk::ImageLayout src_image_layout,
		vk::Format target_format,
		glm::u32vec2 size,
		std::span<std::byte> output_data
	) noexcept
	{
		context.device.waitIdle();

		/* Create resources */

		auto commit_resource_result = CommitResource::create(context.device, context.family);
		if (!commit_resource_result)
			return commit_resource_result.error().forward("Create commit resource failed");
		auto commit_resource = std::move(*commit_resource_result);

		auto transfer_resource_result =
			TransferResource::create(context.allocator, size, output_data.size(), target_format);
		if (!transfer_resource_result)
			return transfer_resource_result.error().forward("Create transfer resource failed");
		auto transfer_resource = std::move(*transfer_resource_result);

		/* Record commands */

		commit_resource.command_buffer.begin(vk::CommandBufferBeginInfo{});
		{
			const auto subresource_range = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor);
			const auto subresource_layer = vulkan::base_level_image_layer(vk::ImageAspectFlagBits::eColor);

			// Pre-blit layout transition
			{
				const auto src_barrier = vk::ImageMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
					.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
					.oldLayout = src_image_layout,
					.newLayout = vk::ImageLayout::eTransferSrcOptimal,
					.image = src_image,
					.subresourceRange = subresource_range
				};

				const auto dst_barrier = vk::ImageMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eNone,
					.srcAccessMask = vk::AccessFlagBits2::eNone,
					.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.oldLayout = vk::ImageLayout::eUndefined,
					.newLayout = vk::ImageLayout::eTransferDstOptimal,
					.image = transfer_resource.blit_dst_image,
					.subresourceRange = subresource_range
				};

				const auto barriers = std::to_array({src_barrier, dst_barrier});
				commit_resource.command_buffer.pipelineBarrier2(
					vk::DependencyInfo().setImageMemoryBarriers(barriers)
				);
			}

			// Blit
			{
				const auto region = std::to_array({
					vk::Offset3D{.x = 0,               .y = 0,               .z = 0},
					vk::Offset3D{.x = int32_t(size.x), .y = int32_t(size.y), .z = 1}
				});
				const auto blit = vk::ImageBlit{
					.srcSubresource = subresource_layer,
					.srcOffsets = region,
					.dstSubresource = subresource_layer,
					.dstOffsets = region
				};

				commit_resource.command_buffer.blitImage(
					src_image,
					vk::ImageLayout::eTransferSrcOptimal,
					transfer_resource.blit_dst_image,
					vk::ImageLayout::eTransferDstOptimal,
					blit,
					vk::Filter::eNearest
				);
			}

			// Post-blit / Pre-copy barriers
			{
				const auto src_barrier = vk::ImageMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
					.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
					.dstAccessMask = vk::AccessFlagBits2::eMemoryRead,
					.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
					.newLayout = src_image_layout,
					.image = src_image,
					.subresourceRange = subresource_range
				};
				const auto dst_barrier = vk::ImageMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
					.oldLayout = vk::ImageLayout::eTransferDstOptimal,
					.newLayout = vk::ImageLayout::eTransferSrcOptimal,
					.image = transfer_resource.blit_dst_image,
					.subresourceRange = subresource_range
				};
				const auto barriers = std::to_array({src_barrier, dst_barrier});
				commit_resource.command_buffer.pipelineBarrier2(
					vk::DependencyInfo().setImageMemoryBarriers(barriers)
				);
			}

			// Copy
			{
				const auto copy_region = vk::BufferImageCopy{
					.imageSubresource = subresource_layer,
					.imageOffset = {.x = 0,          .y = 0,           .z = 0    },
					.imageExtent = {.width = size.x, .height = size.y, .depth = 1}
				};

				commit_resource.command_buffer.copyImageToBuffer(
					transfer_resource.blit_dst_image,
					vk::ImageLayout::eTransferSrcOptimal,
					transfer_resource.readback_buffer,
					copy_region
				);
			}
		}
		commit_resource.command_buffer.end();

		/* Submit */

		const auto command_buffers = std::to_array<vk::CommandBuffer>({*commit_resource.command_buffer});

		{
			const std::scoped_lock lock(context.submit_mutex);
			context.queue.submit(vk::SubmitInfo().setCommandBuffers(command_buffers), *commit_resource.fence);
		}

		/* Wait for fences */

		const auto wait_result =
			context.device.waitForFences(*commit_resource.fence, vk::True, 1'000'000'000);
		switch (wait_result)
		{
		case vk::Result::eSuccess:
			break;
		case vk::Result::eTimeout:
			return Error("Wait for fence timed out");
		default:
			return Error("Wait for fence failed", std::format("vkWaitForFences returned {}", wait_result));
		}

		/* Download data */

		const auto download_result = transfer_resource.readback_buffer.download(output_data);
		if (!download_result) return download_result.error().forward("Download readback buffer failed");

		context.device.waitIdle();

		return {};
	}
}
