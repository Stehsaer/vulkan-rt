#include "vulkan/container/device/frame-buffer.hpp"
#include "common/util/error.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/numeric/base-level.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <libassert/assert.hpp>
#include <ranges>
#include <string>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	std::expected<FrameBuffer, Error> FrameBuffer::create_color(
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
			.usage =
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | additional_usage
		};

		auto image_result = allocator.create_image(image_create_info, vulkan::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create color buffer failed");
		auto image = std::move(image_result.value());

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		auto view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!view_result) return view_result.error().forward("Create color buffer view failed");
		auto view = std::move(view_result.value());

		return FrameBuffer(format, std::move(image), std::move(view));
	}

	std::expected<FrameBuffer, Error> FrameBuffer::create_depth(
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
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | additional_usage
		};

		auto image_result = allocator.create_image(image_create_info, vulkan::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create depth buffer failed");
		auto image = std::move(image_result.value());

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eDepth)
		};

		auto view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!view_result) return view_result.error().forward("Create depth buffer view failed");
		auto view = std::move(view_result.value());

		return FrameBuffer(format, std::move(image), std::move(view));
	}

	std::expected<FrameBuffer, Error> FrameBuffer::create_depth_stencil(
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
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment
				| vk::ImageUsageFlagBits::eSampled
				| additional_usage
		};

		auto image_result = allocator.create_image(image_create_info, vulkan::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create depth-stencil buffer failed");
		auto image = std::move(image_result.value());

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = vulkan::base_level_image_range(
				vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil
			)
		};

		auto view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!view_result) return view_result.error().forward("Create depth-stencil buffer view failed");
		auto view = std::move(view_result.value());

		return FrameBuffer(format, std::move(image), std::move(view));
	}

	namespace
	{
		// returns (depth, stencil)
		std::pair<bool, bool> has_depth_stencil(vk::Format format) noexcept
		{
			const auto component_names =
				std::views::iota(0u, vk::componentCount(format))
				| std::views::transform([format](uint32_t comp) {
					  return std::string(vk::componentName(format, comp));
				  });

			return {std::ranges::contains(component_names, "D"), std::ranges::contains(component_names, "S")};
		}

		vk::ImageAspectFlags get_image_aspects(std::pair<bool, bool> depth_stencil) noexcept
		{
			if (!depth_stencil.first) return vk::ImageAspectFlagBits::eColor;
			if (!depth_stencil.second) return vk::ImageAspectFlagBits::eDepth;
			return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
		}

		vk::ImageUsageFlags get_image_usages(
			std::pair<bool, bool> depth_stencil,
			vk::ImageUsageFlags additional_usage
		) noexcept
		{
			return (depth_stencil.first ? vk::ImageUsageFlagBits::eDepthStencilAttachment
										: vk::ImageUsageFlagBits::eColorAttachment)
				| additional_usage
				| vk::ImageUsageFlagBits::eSampled;
		}
	}

	std::expected<FrameBuffer, Error> FrameBuffer::create(
		const vk::raii::Device& device,
		const vulkan::Allocator& allocator,
		glm::u32vec2 extent,
		vk::Format format,
		vk::ImageUsageFlags additional_usage
	) noexcept
	{
		const auto depth_stencil_flags = has_depth_stencil(format);

		const auto image_create_info = vk::ImageCreateInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = {.width = extent.x, .height = extent.y, .depth = 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = get_image_usages(depth_stencil_flags, additional_usage)
		};

		auto image_result = allocator.create_image(image_create_info, vulkan::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create depth-stencil buffer failed");
		auto image = std::move(image_result.value());

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = vulkan::base_level_image_range(get_image_aspects(depth_stencil_flags))
		};

		auto view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!view_result) return view_result.error().forward("Create depth-stencil buffer view failed");
		auto view = std::move(view_result.value());

		return FrameBuffer(format, std::move(image), std::move(view));
	}
}
