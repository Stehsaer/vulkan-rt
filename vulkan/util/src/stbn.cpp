#include "vulkan/util/stbn.hpp"
#include "common/util/error.hpp"
#include "image/noise.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/util/command-runner.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <span>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	std::expected<STBN, Error> STBN::create(const vulkan::Context& context) noexcept
	{
		/*===== Allocate =====*/

		const auto image_create_info = vk::ImageCreateInfo{
			.imageType = vk::ImageType::e3D,
			.format = vk::Format::eR8G8Unorm,
			.extent = {.width = 128, .height = 128, .depth = 64},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			.sharingMode = vk::SharingMode::eExclusive,
			.initialLayout = vk::ImageLayout::eUndefined,
		};

		const auto staging_buffer_create_info = vk::BufferCreateInfo{
			.size = image::stbn_data.size() * sizeof(glm::u8vec2),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
			.sharingMode = vk::SharingMode::eExclusive,
		};

		auto image_result = context.allocator.create_image(image_create_info, MemoryUsage::GpuOnly);
		auto staging_buffer_result =
			context.allocator.create_buffer(staging_buffer_create_info, MemoryUsage::CpuToGpu);

		if (!image_result) return image_result.error().forward("Create STBN image failed");
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffer failed");

		auto image = std::move(*image_result);
		auto staging_buffer = std::move(*staging_buffer_result);

		/*===== Copy =====*/

		if (const auto result = staging_buffer.upload(
				std::as_bytes(std::span(image::stbn_data.data_handle(), image::stbn_data.size()))
			);
			!result)
			return result.error().forward("Copy data to staging buffer failed");

		/*===== Upload =====*/

		auto runner_result = CommandRunner::create(context);
		if (!runner_result) return runner_result.error().forward("Create command runner failed");
		auto runner = std::move(*runner_result);

		const auto pre_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor),
		};

		const auto post_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor),
		};

		const auto buffer_image_copy = vk::BufferImageCopy{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = vulkan::base_level_image_layer(vk::ImageAspectFlagBits::eColor),
			.imageOffset = {.x = 0,       .y = 0,        .z = 0     },
			.imageExtent = {.width = 128, .height = 128, .depth = 64},
		};

		const auto run_result = runner.run(context, [&](const vk::raii::CommandBuffer& command_buffer) {
			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
			command_buffer.copyBufferToImage(
				staging_buffer,
				image,
				vk::ImageLayout::eTransferDstOptimal,
				buffer_image_copy
			);
			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_barrier));
		});
		if (!run_result) return run_result.error().forward("Run copy commands failed");

		/*===== Image View =====*/

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e3D,
			.format = vk::Format::eR8G8Unorm,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor),
		};
		auto image_view_result = context.device.createImageView(image_view_create_info);
		if (!image_view_result) return Error::from(image_view_result);
		auto image_view = std::move(*image_view_result);

		return STBN{
			.image = std::move(image),
			.view = std::move(image_view),
		};
	}
}
