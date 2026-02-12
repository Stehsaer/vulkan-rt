#include "vulkan/context.hpp"

#include "impl/device.hpp"
#include "impl/instance.hpp"
#include "impl/swapchain.hpp"
#include "impl/window.hpp"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>

namespace vulkan
{
	std::expected<Context, Error> Context::create(const CreateInfo& create_info) noexcept
	{
		auto window_result = context::create_window(create_info.window_info);
		if (!window_result) return window_result.error().forward("Create window failed");
		auto window = std::move(*window_result);

		auto context = vk::raii::Context{};

		auto instance_result = context::create_instance(context, create_info.app_info, create_info.features);
		if (!instance_result) return instance_result.error().forward("Create Vulkan instance failed");
		auto instance = std::move(*instance_result);

		auto surface_result = context::create_surface(instance, *window);
		if (!surface_result) return surface_result.error().forward("Create surface failed");
		auto surface = std::move(*surface_result);

		auto phy_device_result = context::pick_physical_device(instance, create_info.features);
		if (!phy_device_result) return phy_device_result.error().forward("Pick physical device failed");
		auto phy_device = std::move(*phy_device_result);

		auto device_and_queues_result =
			context::create_logical_device(phy_device, vk::SurfaceKHR(*surface), create_info.features);
		if (!device_and_queues_result)
			return device_and_queues_result.error().forward("Create logical device failed");
		auto device_and_queues = std::move(*device_and_queues_result);
		auto [device, queues] = std::move(device_and_queues);

		auto swapchain_layout_result =
			context::select_swapchain_layout(phy_device, vk::SurfaceKHR(*surface), queues);
		if (!swapchain_layout_result)
			return swapchain_layout_result.error().forward("Select swapchain layout failed");
		auto swapchain_layout = std::move(*swapchain_layout_result);

		auto allocator_result = vulkan::alloc::Allocator::create(instance, phy_device, device);
		if (!allocator_result) return allocator_result.error().forward("Create allocator failed");
		auto allocator = std::move(*allocator_result);

		return Context(
			std::move(window),
			std::move(context),
			std::move(instance),
			std::move(surface),
			std::move(phy_device),
			std::move(device),
			std::move(queues),
			std::move(swapchain_layout),
			std::move(allocator)
		);
	}

	std::expected<vulkan::SwapchainAcquireResult, Error> Context::acquire_swapchain_image(
		std::optional<vk::Semaphore> signal_semaphore,
		std::optional<vk::Fence> signal_fence,
		uint64_t timeout
	) noexcept
	{
		while (true)
		{
			if (!swapchain)
			{
				auto new_swapchain_result = vulkan::SwapchainInstance::create(
					phy_device,
					device,
					vk::SurfaceKHR(*surface),
					swapchain_layout,
					std::move(swapchain)
				);
				if (!new_swapchain_result)
					return new_swapchain_result.error().forward("Recreate swapchain failed");
				auto new_swapchain = std::move(*new_swapchain_result);
				swapchain = std::move(new_swapchain);

				continue;
			}

			const auto acquire_expected =
				swapchain->acquire_next_image(signal_semaphore, signal_fence, timeout);
			if (!acquire_expected)
			{
				switch (acquire_expected.error())
				{
				case vk::Result::eErrorOutOfDateKHR:
				case vk::Result::eSuboptimalKHR:
				{
					device.waitIdle();
					auto new_swapchain_result = vulkan::SwapchainInstance::create(
						phy_device,
						device,
						vk::SurfaceKHR(*surface),
						swapchain_layout,
						std::move(swapchain)
					);
					if (!new_swapchain_result)
						return new_swapchain_result.error().forward("Recreate swapchain failed");
					auto new_swapchain = std::move(*new_swapchain_result);
					swapchain = std::move(new_swapchain);

					continue;
				}
				case vk::Result::eNotReady:
					continue;
				default:
					return acquire_expected.transform_error(Error::from<vk::Result>())
						.error()
						.forward("Acquire swapchain image failed");
				}
			}

			return *acquire_expected;
		}
	}

	std::expected<void, Error> Context::present_swapchain_image(
		uint32_t image_index,
		std::optional<vk::Semaphore> wait_semaphore
	) noexcept
	{
		assert(swapchain.has_value());

		std::vector<vk::Semaphore> wait_semaphores;
		if (wait_semaphore) wait_semaphores.push_back(*wait_semaphore);

		const auto swapchains = std::to_array({swapchain->get_swapchain()});
		const auto image_indices = std::to_array({image_index});

		const auto present_info =
			vk::PresentInfoKHR{}
				.setImageIndices(image_indices)
				.setWaitSemaphores(wait_semaphores)
				.setSwapchains(swapchains);

		const auto present_result = queues.present->presentKHR(present_info);
		switch (present_result)
		{
		case vk::Result::eSuccess:
			return {};

		case vk::Result::eErrorOutOfDateKHR:
		case vk::Result::eSuboptimalKHR:
		{
			{
				auto new_swapchain_result = vulkan::SwapchainInstance::create(
					phy_device,
					device,
					vk::SurfaceKHR(*surface),
					swapchain_layout,
					std::move(swapchain)
				);
				if (!new_swapchain_result)
					return new_swapchain_result.error().forward("Recreate swapchain failed");
				auto new_swapchain = std::move(*new_swapchain_result);
				swapchain = std::move(new_swapchain);
			}
			return {};
		}
		default:
			return Error::from<vk::Result>()(present_result).forward("Present swapchain image failed");
		}
	}

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

		auto image_view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!image_view_result)
			return image_view_result.error().forward("Create image view for swapchain image failed");
		auto image_view = std::move(*image_view_result);

		return image_view;
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

		auto swapchain_result =
			device.createSwapchainKHR(swapchain_create_info).transform_error(Error::from<vk::Result>());
		if (!swapchain_result) return swapchain_result.error().forward("Create swapchain failed");
		auto swapchain = std::move(*swapchain_result);

		const auto images = swapchain.getImages();

		std::vector<SwapchainImage> swapchain_images;
		for (const auto& image : images)
		{
			auto image_view_result =
				view_from_swapchain_img(device, image, swapchain_layout.surface_format.format);
			if (!image_view_result)
				return image_view_result.error().forward("Create image view for swapchain image failed");
			auto image_view = std::move(*image_view_result);

			swapchain_images.push_back({.image = image, .view = std::move(image_view)});
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

		return SwapchainAcquireResult{
			.extent = extent,
			.image_index = idx,
			.image = images.at(idx).image,
			.image_view = images.at(idx).view
		};
	}

	SurfaceWrapper::~SurfaceWrapper() noexcept
	{
		SDL_Vulkan_DestroySurface(instance, surface, nullptr);
	}

	WindowWrapper::~WindowWrapper() noexcept
	{
		SDL_DestroyWindow(window);
		SDL_Vulkan_UnloadLibrary();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}