#include "common/util/error.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/container/device/attachment.hpp"
#include "vulkan/numeric/base-level.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
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
			.usage = get_image_usages(format, additional_usage)
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
}
