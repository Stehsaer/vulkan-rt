#include "vulkan/util/static-resource-creator.hpp"
#include "common/formatter.hpp"
#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "common/util/span.hpp"
#include "image/bc-image.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/alloc/image.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <glm/ext/vector_uint2_sized.hpp>
#include <glm/vector_relational.hpp>
#include <limits>
#include <mutex>
#include <ranges>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace vulkan
{
#pragma region Utility
	std::expected<Buffer, Error> StaticResourceCreator::create_staging_buffer(
		const Context& context,
		std::span<const std::byte> data
	) noexcept
	{
		auto staging_buffer_result = context.allocator.create_buffer(
			vk::BufferCreateInfo{.size = data.size_bytes(), .usage = vk::BufferUsageFlagBits::eTransferSrc},
			MemoryUsage::CpuToGpu
		);
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffer failed");
		auto staging_buffer = std::move(*staging_buffer_result);

		const auto upload_result = staging_buffer.upload(data);
		if (!upload_result) return upload_result.error().forward("Upload data to staging buffer failed");

		return staging_buffer;
	}

	std::expected<void, Error> StaticResourceCreator::check_mipmap_chain_sizes(
		std::vector<glm::u32vec2> sizes
	) noexcept
	{
		for (const auto [idx, pair] : sizes | std::views::adjacent<2> | std::views::enumerate)
		{
			const auto [upper_size, lower_size] = pair;
			if (!glm::all(glm::equal(upper_size / 2_u32, lower_size)))
			{
				return Error(
					"Invalid mipmap chain sizes",
					std::format("[Level {}] {} -> [Level {}] {}", idx, upper_size, idx + 1, lower_size)
				);
			}
		}

		return {};
	}

	static vk::Format get_bcn_format(image::BCnFormat format, bool srgb)
	{
		switch (format)
		{
		case image::BCnFormat::BC3:
			return srgb ? vk::Format::eBc3SrgbBlock : vk::Format::eBc3UnormBlock;

		case image::BCnFormat::BC5:
			return vk::Format::eBc5UnormBlock;

		case image::BCnFormat::BC7:
			return srgb ? vk::Format::eBc7SrgbBlock : vk::Format::eBc7UnormBlock;
		}

		return vk::Format::eUndefined;
	}

	vk::ImageMemoryBarrier2 StaticResourceCreator::ImageUploadTask::get_barrier_pre() const
	{
		const auto subresource_range = vk::ImageSubresourceRange{
			.aspectMask = subresource_layers.aspectMask,
			.baseMipLevel = subresource_layers.mipLevel,
			.levelCount = 1,
			.baseArrayLayer = subresource_layers.baseArrayLayer,
			.layerCount = subresource_layers.layerCount,
		};

		return vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = dst_image,
			.subresourceRange = subresource_range,
		};
	}

	vk::ImageMemoryBarrier2 StaticResourceCreator::ImageUploadTask::get_barrier_post() const
	{
		const auto subresource_range = vk::ImageSubresourceRange{
			.aspectMask = subresource_layers.aspectMask,
			.baseMipLevel = subresource_layers.mipLevel,
			.levelCount = 1,
			.baseArrayLayer = subresource_layers.baseArrayLayer,
			.layerCount = 1
		};

		return vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = dst_layout,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = dst_image,
			.subresourceRange = subresource_range,
		};
	}

#pragma endregion

#pragma region Creation

	std::expected<Buffer, Error> StaticResourceCreator::create_buffer(
		const Context& context,
		std::span<const std::byte> data,
		vk::BufferUsageFlags usage
	) noexcept
	{
		auto staging_buffer_result = create_staging_buffer(context, data);
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffer failed");
		auto staging_buffer = std::move(*staging_buffer_result);

		auto dst_buffer_result = context.allocator.create_buffer(
			vk::BufferCreateInfo{
				.size = data.size_bytes(),
				.usage = usage | vk::BufferUsageFlagBits::eTransferDst
			},
			MemoryUsage::GpuOnly
		);
		if (!dst_buffer_result) return dst_buffer_result.error().forward("Create gpu buffer failed");
		auto dst_buffer = std::move(*dst_buffer_result);

		const std::scoped_lock lock(*execution_mutex);
		pending_data_size += data.size_bytes();
		buffer_upload_tasks.push_back(
			BufferUploadTask{
				.dst_buffer = dst_buffer,
				.staging_buffer = std::move(staging_buffer),
				.data_size = data.size_bytes()
			}
		);

		return dst_buffer;
	}

	std::expected<Image, Error> StaticResourceCreator::create_image_bcn(
		const Context& context,
		const image::BCnImage& image,
		bool srgb,
		vk::ImageUsageFlags usage,
		vk::ImageLayout layout,
		vk::ImageCreateFlags create_flags
	) noexcept
	{
		const auto extent = vk::Extent3D{.width = image.size.x * 4, .height = image.size.y * 4, .depth = 1};
		const auto subresource_layer = vulkan::base_level_image_layer(vk::ImageAspectFlagBits::eColor);

		const auto image_create_info = vk::ImageCreateInfo{
			.flags = create_flags,
			.imageType = vk::ImageType::e2D,
			.format = get_bcn_format(image.format, srgb),
			.extent = extent,
			.mipLevels = 1,
			.arrayLayers = 1,
			.usage = usage | vk::ImageUsageFlagBits::eTransferDst
		};
		auto image_result = context.allocator.create_image(image_create_info, vulkan::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create gpu image failed");
		auto dst_image = std::move(*image_result);

		auto staging_buffer_result = create_staging_buffer(context, util::as_bytes(image.data));
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffer failed");

		const std::scoped_lock lock(*execution_mutex);
		pending_data_size += std::span(image.data).size_bytes();
		image_upload_tasks.push_back(
			ImageUploadTask{
				.dst_image = dst_image,
				.staging_buffer = std::move(*staging_buffer_result),
				.subresource_layers = subresource_layer,
				.image_extent = extent,
				.dst_layout = layout
			}
		);

		return dst_image;
	}

	std::expected<Image, Error> StaticResourceCreator::create_image_mipmap_bcn(
		const Context& context,
		const std::span<const image::BCnImage>& mipmap_chain,
		bool srgb,
		vk::ImageUsageFlags usage,
		vk::ImageLayout layout,
		vk::ImageCreateFlags create_flags
	) noexcept
	{
		/* Verify inputs */

		// Empty mipmap chain
		if (mipmap_chain.empty()) return Error("Input mipmap chain is empty");

		// Verify sizes
		if (const auto size_check_result = check_mipmap_chain_sizes(
				mipmap_chain
				| std::views::transform([](const auto& image) { return image.size; })
				| std::ranges::to<std::vector>()
			);
			!size_check_result)
		{
			return size_check_result.error();
		}

		// Verify formats
		const auto expected_format = mipmap_chain.front().format;
		if (!std::ranges::all_of(mipmap_chain, [expected_format](const auto& image) {
				return image.format == expected_format;
			}))
		{
			return Error("Inconsistent formats in mipmap chain");
		}

		/* Create image */

		const std::vector<vk::Extent3D> extents =
			mipmap_chain
			| std::views::transform([](const auto& image) {
				  return vk::Extent3D{.width = image.size.x * 4, .height = image.size.y * 4, .depth = 1};
			  })
			| std::ranges::to<std::vector>();
		const uint32_t mipmap_levels = mipmap_chain.size();

		const auto image_create_info = vk::ImageCreateInfo{
			.flags = create_flags,
			.imageType = vk::ImageType::e2D,
			.format = get_bcn_format(mipmap_chain.front().format, srgb),
			.extent = extents[0],
			.mipLevels = mipmap_levels,
			.arrayLayers = 1,
			.usage = usage | vk::ImageUsageFlagBits::eTransferDst
		};
		auto image_result = context.allocator.create_image(image_create_info, vulkan::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create gpu image failed");
		auto dst_image = std::move(*image_result);

		/* Create staging buffers */

		auto staging_buffer_result =
			mipmap_chain
			| std::views::transform([](const auto& image) { return util::as_bytes(image.data); })
			| std::views::transform([this, &context](auto data) {
				  return create_staging_buffer(context, data);
			  })
			| Error::collect();
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffers failed");
		std::vector<Buffer> staging_buffers = std::move(*staging_buffer_result);

		/* Append tasks */

		const std::vector<vk::ImageSubresourceLayers> subresource_layers =
			std::views::iota(0_u32, mipmap_levels)
			| std::views::transform([](auto mip_level) {
				  return vk::ImageSubresourceLayers{
					  .aspectMask = vk::ImageAspectFlagBits::eColor,
					  .mipLevel = static_cast<uint32_t>(mip_level),
					  .baseArrayLayer = 0,
					  .layerCount = 1,
				  };
			  })
			| std::ranges::to<std::vector>();

		const auto as_upload_task =
			[&dst_image, layout](const auto& extent, const auto& subresource_layer, auto& staging_buffer) {
				return ImageUploadTask{
					.dst_image = dst_image,
					.staging_buffer = std::move(staging_buffer),
					.subresource_layers = subresource_layer,
					.image_extent = extent,
					.dst_layout = layout
				};
			};

		const std::scoped_lock lock(*execution_mutex);
		pending_data_size +=
			std::ranges::fold_left_first(
				mipmap_chain | std::views::transform([](const auto& image) {
					return std::span(image.data).size_bytes();
				}),
				std::plus()
			)
				.value_or(0);
		image_upload_tasks.append_range(
			std::views::zip_transform(as_upload_task, extents, subresource_layers, staging_buffers)
		);

		return dst_image;
	}

#pragma endregion

	size_t StaticResourceCreator::num_pending() const noexcept
	{
		const std::scoped_lock lock(*execution_mutex);
		return buffer_upload_tasks.size() + image_upload_tasks.size();
	}

	size_t StaticResourceCreator::size_pending() const noexcept
	{
		const std::scoped_lock lock(*execution_mutex);
		return pending_data_size;
	}

	std::expected<void, Error> StaticResourceCreator::execute_uploads_with_size_thres(
		const Context& context,
		size_t size_thres
	) noexcept
	{
		// Note: these buffers has 0 size before use, no extra alloc
		std::vector<BufferUploadTask> buffer_tasks;
		std::vector<ImageUploadTask> image_tasks;

		{
			const std::scoped_lock lock(*execution_mutex);
			if (pending_data_size < size_thres) return {};

			// Use std::exchange instead of std::move to avoid leaving moved-away objects
			buffer_tasks = std::exchange(buffer_upload_tasks, {});
			image_tasks = std::exchange(image_upload_tasks, {});
			pending_data_size = 0;
		}

		return execute_uploads_impl(context, buffer_tasks, image_tasks);
	}

	std::expected<void, Error> StaticResourceCreator::execute_uploads(const Context& context) noexcept
	{
		// Note: these buffers has 0 size before use, no extra alloc
		std::vector<BufferUploadTask> buffer_tasks;
		std::vector<ImageUploadTask> image_tasks;

		{
			const std::scoped_lock lock(*execution_mutex);

			// Use std::exchange instead of std::move to avoid leaving moved-away objects
			buffer_tasks = std::exchange(buffer_upload_tasks, {});
			image_tasks = std::exchange(image_upload_tasks, {});
			pending_data_size = 0;
		}

		return execute_uploads_impl(context, buffer_tasks, image_tasks);
	}

	std::expected<void, Error> StaticResourceCreator::execute_uploads_impl(
		const Context& context,
		const std::vector<BufferUploadTask>& buffer_tasks,
		const std::vector<ImageUploadTask>& image_tasks
	) noexcept
	{
		if (buffer_tasks.empty() && image_tasks.empty()) return {};

		/* Create command pool */

		auto command_pool_result = context.device.createCommandPool(
			{.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = context.family}
		);
		if (!command_pool_result) return Error::from(command_pool_result);
		auto command_pool = std::move(*command_pool_result);

		/* Create command buffer */

		auto allocated_command_buffers_result = context.device.allocateCommandBuffers({
			.commandPool = command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		});
		if (!allocated_command_buffers_result) return Error::from(allocated_command_buffers_result);
		auto allocated_command_buffers = std::move(*allocated_command_buffers_result);
		auto command_buffer = std::move(allocated_command_buffers[0]);

		/* Create fence */

		auto fence_result = context.device.createFence({});
		if (!fence_result) return Error::from(fence_result);
		auto fence = std::move(*fence_result);

		/* Synchronization infos */

		const auto buffer_barrier_post = vk::MemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
		};

		const auto image_barriers_pre = image_tasks
			| std::views::transform([](const auto& task) { return task.get_barrier_pre(); })
			| std::ranges::to<std::vector>();

		const auto image_barriers_post = image_tasks
			| std::views::transform([](const auto& task) { return task.get_barrier_post(); })
			| std::ranges::to<std::vector>();

		/* Record commands */

		if (const auto result =
				command_buffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
			!result)
			return Error::from(result);
		{
			// Copy buffers
			for (const auto& task : buffer_tasks)
			{
				const auto copy_region = vk::BufferCopy{
					.srcOffset = 0,
					.dstOffset = 0,
					.size = task.data_size,
				};
				command_buffer.copyBuffer(task.staging_buffer, task.dst_buffer, copy_region);
			}

			// Barrier after buffer copies and before image copies
			{
				auto dependecy_info = vk::DependencyInfo();
				if (!buffer_tasks.empty()) dependecy_info.setMemoryBarriers(buffer_barrier_post);
				if (!image_tasks.empty()) dependecy_info.setImageMemoryBarriers(image_barriers_pre);
				command_buffer.pipelineBarrier2(dependecy_info);
			}

			// Copy images
			for (const auto& task : image_tasks)
			{
				const auto buffer_image_copy = vk::BufferImageCopy{
					.bufferOffset = 0,
					.bufferRowLength = 0,
					.bufferImageHeight = 0,
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

			// Barrier after image copies
			if (!image_tasks.empty())
				command_buffer.pipelineBarrier2(
					vk::DependencyInfo{}.setImageMemoryBarriers(image_barriers_post)
				);
		}
		if (const auto result = command_buffer.end(); !result) return Error::from(result);

		/* Submit and wait for results */

		const auto command_buffers = std::to_array<vk::CommandBuffer>({command_buffer});
		const auto submit_info = vk::SubmitInfo{}.setCommandBuffers(command_buffers);

		{
			const std::scoped_lock lock(context.submit_mutex);
			if (const auto result = context.queue.submit(submit_info, fence); !result)
				return Error::from(result);
		}

		if (const auto result =
				context.device.waitForFences({fence}, vk::True, std::numeric_limits<uint64_t>::max());
			result != vk::Result::eSuccess)
			return Error::from(result);

		return {};
	}
}
