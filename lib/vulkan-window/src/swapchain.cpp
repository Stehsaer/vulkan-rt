#include "vulkan-window/swapchain.hpp"

namespace vulkan_window
{
	static std::expected<vk::raii::ImageView, Error> view_from_swapchain_img(
		const vk::raii::Device& device,
		const vk::Image& image,
		vk::Format format
	) noexcept
	{
		const auto components = vk::ComponentMapping{
			.r = vk::ComponentSwizzle::eIdentity,
			.g = vk::ComponentSwizzle::eIdentity,
			.b = vk::ComponentSwizzle::eIdentity,
			.a = vk::ComponentSwizzle::eIdentity
		};

		const auto subresource_range = vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.components = components,
			.subresourceRange = subresource_range
		};

		auto image_view_expected = device.createImageView(image_view_create_info);
		if (!image_view_expected)
			return Error(image_view_expected.error(), "Create image view from swapchain image failed");

		return std::move(*image_view_expected);
	}

	std::expected<SwapchainInstance, Error> SwapchainInstance::create(
		const vk::raii::PhysicalDevice& phy_device,
		const vk::raii::Device& device,
		vk::SurfaceKHR surface,
		const SwapchainLayout& swapchain_layout,
		std::optional<SwapchainInstance> old_swapchain
	) noexcept
	{
		const auto surface_capability = phy_device.getSurfaceCapabilitiesKHR(surface);

		const auto image_count =
			glm::clamp<uint32_t>(3, surface_capability.minImageCount, surface_capability.maxImageCount);

		const auto prev_swapchain =
			old_swapchain
				.transform([](const SwapchainInstance& swapchain) -> vk::SwapchainKHR {
					return swapchain.swapchain;
				})
				.value_or(nullptr);

		auto swapchain_create_info = vk::SwapchainCreateInfoKHR{
			.flags = {},
			.surface = surface,
			.minImageCount = image_count,
			.imageFormat = swapchain_layout.surface_format.format,
			.imageColorSpace = swapchain_layout.surface_format.colorSpace,
			.imageExtent = surface_capability.currentExtent,
			.imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
			.imageSharingMode = swapchain_layout.image_sharing_mode,
			.preTransform = surface_capability.currentTransform,
			.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
			.presentMode = swapchain_layout.present_mode,
			.clipped = vk::True,
			.oldSwapchain = prev_swapchain
		};
		swapchain_create_info.setQueueFamilyIndices(swapchain_layout.image_queue_family_indices);

		auto swapchain_expected = device.createSwapchainKHR(swapchain_create_info);
		if (!swapchain_expected) return Error(swapchain_expected.error(), "Create swapchain failed");
		auto swapchain = std::move(*swapchain_expected);

		const auto images = swapchain.getImages();

		std::vector<SwapchainImage> swapchain_images;
		for (const auto& image : images)
		{
			auto image_view_expected =
				view_from_swapchain_img(device, image, swapchain_layout.surface_format.format);
			if (!image_view_expected)
				return image_view_expected.error().forward("Create image view for swapchain image failed");

			swapchain_images.push_back({.image = image, .view = std::move(*image_view_expected)});
		}

		return SwapchainInstance(
			std::move(swapchain_images),
			std::move(swapchain),
			glm::u32vec2(swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height)
		);
	}

	std::expected<SwapchainAcquireResult, vk::Result> SwapchainInstance::acquire_next_image(
		std::optional<vk::Semaphore> semaphore,
		std::optional<vk::Fence> fence,
		uint64_t timeout
	) noexcept
	{
		const auto [result, idx] =
			swapchain.acquireNextImage(timeout, semaphore.value_or(nullptr), fence.value_or(nullptr));

		if (result != vk::Result::eSuccess) return std::unexpected(result);

		return SwapchainAcquireResult{.extent = extent, .image_index = idx, .image = images.at(idx)};
	}
}