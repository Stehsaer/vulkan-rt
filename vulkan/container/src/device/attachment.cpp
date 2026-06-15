#include "vulkan/container/device/attachment.hpp"
#include "common/util/error.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/numeric/base-level.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int4_sized.hpp>
#include <glm/ext/vector_uint2_sized.hpp>
#include <glm/ext/vector_uint4_sized.hpp>
#include <libassert/assert.hpp>
#include <string>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	namespace
	{
		vk::ImageAspectFlags get_image_aspects(vk::Format format) noexcept
		{
			if (!vk::hasDepthComponent(format)) return vk::ImageAspectFlagBits::eColor;
			if (!vk::hasStencilComponent(format)) return vk::ImageAspectFlagBits::eDepth;
			return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
		}

		vk::ImageUsageFlags get_image_usages(vk::Format format, vk::ImageUsageFlags additional_usage) noexcept
		{
			return (vk::hasDepthComponent(format)
						? vk::ImageUsageFlagBits::eDepthStencilAttachment
						: vk::ImageUsageFlagBits::eColorAttachment)
				| additional_usage
				| vk::ImageUsageFlagBits::eSampled;
		}
	}

	std::expected<Attachment, Error> Attachment::create(
		const vk::raii::Device& device,
		const vulkan::Allocator& allocator,
		glm::u32vec2 extent,
		vk::Format format,
		vk::ImageUsageFlags additional_usage
	) noexcept
	{
		const auto image_create_info = vk::ImageCreateInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = {.width = extent.x, .height = extent.y, .depth = 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = get_image_usages(format, additional_usage) | vk::ImageUsageFlagBits::eTransferDst
		};

		auto image_result = allocator.create_image(image_create_info, vulkan::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create image failed");
		auto image = std::move(image_result.value());

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = vulkan::base_level_image_range(get_image_aspects(format))
		};

		auto view_result = device.createImageView(image_view_create_info);
		if (!view_result) return Error::from(view_result);
		auto view = std::move(view_result.value());

		return Attachment(format, std::move(image), std::move(view));
	}

	static vk::ImageMemoryBarrier2 pre_barrier(vk::Image image, vk::ImageAspectFlags aspect) noexcept
	{
		return {
			.srcStageMask = {},
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = vulkan::base_level_image_range(aspect)
		};
	};

	static vk::ImageMemoryBarrier2 post_barrier(
		vk::Image image,
		vk::ImageAspectFlags aspect,
		vk::ImageLayout dst_layout
	) noexcept
	{
		return {
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
			.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = dst_layout,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = vulkan::base_level_image_range(aspect)
		};
	};

	void Attachment::clear_color_float(
		const vk::raii::CommandBuffer& command_buffer,
		glm::vec4 color,
		vk::ImageLayout layout
	) const noexcept
	{
		DEBUG_ASSERT(vk::hasRedComponent(format));

		const auto pre_image_barrier = pre_barrier(image, vk::ImageAspectFlagBits::eColor);
		const auto post_image_barrier = post_barrier(image, vk::ImageAspectFlagBits::eColor, layout);

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_image_barrier));
		command_buffer.clearColorImage(
			image,
			vk::ImageLayout::eTransferDstOptimal,
			{color.x, color.y, color.z, color.a},
			vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_image_barrier));
	}

	void Attachment::clear_color_uint(
		const vk::raii::CommandBuffer& command_buffer,
		glm::u32vec4 color,
		vk::ImageLayout layout
	) const noexcept
	{
		DEBUG_ASSERT(vk::hasRedComponent(format), "Format must be a color format");

		const auto pre_image_barrier = pre_barrier(image, vk::ImageAspectFlagBits::eColor);
		const auto post_image_barrier = post_barrier(image, vk::ImageAspectFlagBits::eColor, layout);

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_image_barrier));
		command_buffer.clearColorImage(
			image,
			vk::ImageLayout::eTransferDstOptimal,
			{color.x, color.y, color.z, color.a},
			vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_image_barrier));
	}

	void Attachment::clear_color_int(
		const vk::raii::CommandBuffer& command_buffer,
		glm::i32vec4 color,
		vk::ImageLayout layout
	) const noexcept
	{
		DEBUG_ASSERT(vk::hasRedComponent(format), "Format must be a color format");

		const auto pre_image_barrier = pre_barrier(image, vk::ImageAspectFlagBits::eColor);
		const auto post_image_barrier = post_barrier(image, vk::ImageAspectFlagBits::eColor, layout);

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_image_barrier));
		command_buffer.clearColorImage(
			image,
			vk::ImageLayout::eTransferDstOptimal,
			{color.x, color.y, color.z, color.a},
			vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_image_barrier));
	}

	void Attachment::clear_depth_stencil(
		const vk::raii::CommandBuffer& command_buffer,
		float depth,
		uint8_t stencil,
		vk::ImageLayout layout
	) const noexcept
	{
		DEBUG_ASSERT(
			vk::hasDepthComponent(format) || vk::hasStencilComponent(format),
			"Format must contains depth or stencil component"
		);

		vk::ImageAspectFlags aspect = {};
		if (vk::hasDepthComponent(format)) aspect |= vk::ImageAspectFlagBits::eDepth;
		if (vk::hasStencilComponent(format)) aspect |= vk::ImageAspectFlagBits::eStencil;

		const auto pre_image_barrier = pre_barrier(image, aspect);
		const auto post_image_barrier = post_barrier(image, aspect, layout);

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_image_barrier));
		command_buffer.clearDepthStencilImage(
			image,
			vk::ImageLayout::eTransferDstOptimal,
			{.depth = depth, .stencil = stencil},
			vulkan::base_level_image_range(aspect)
		);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_image_barrier));
	}
}
