#include "vulkan/util/frame-buffer.hpp"
#include "vulkan/util/constants.hpp"

namespace vulkan
{
	std::expected<FrameBuffer, Error> FrameBuffer::create_color(
		const vk::raii::Device& device,
		const vulkan::alloc::Allocator& allocator,
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

		auto image_result = allocator.create_image(image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create color buffer failed");
		auto image = std::move(image_result.value());

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = vulkan::base_level_image(vk::ImageAspectFlagBits::eColor)
		};

		auto view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!view_result) return view_result.error().forward("Create color buffer view failed");
		auto view = std::move(view_result.value());

		return FrameBuffer{.image = std::move(image), .view = std::move(view)};
	}

	std::expected<FrameBuffer, Error> FrameBuffer::create_depth(
		const vk::raii::Device& device,
		const vulkan::alloc::Allocator& allocator,
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

		auto image_result = allocator.create_image(image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create depth buffer failed");
		auto image = std::move(image_result.value());

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = vulkan::base_level_image(vk::ImageAspectFlagBits::eDepth)
		};

		auto view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!view_result) return view_result.error().forward("Create depth buffer view failed");
		auto view = std::move(view_result.value());

		return FrameBuffer{.image = std::move(image), .view = std::move(view)};
	}

	std::expected<FrameBuffer, Error> FrameBuffer::create_depth_stencil(
		const vk::raii::Device& device,
		const vulkan::alloc::Allocator& allocator,
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

		auto image_result = allocator.create_image(image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create depth-stencil buffer failed");
		auto image = std::move(image_result.value());

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange =
				vulkan::base_level_image(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)
		};

		auto view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!view_result) return view_result.error().forward("Create depth-stencil buffer view failed");
		auto view = std::move(view_result.value());

		return FrameBuffer{.image = std::move(image), .view = std::move(view)};
	}
}
