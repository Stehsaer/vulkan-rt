#include "vulkan/context/swapchain.hpp"
#include "common/util/variant.hpp"
#include "vulkan/util/glm.hpp"

#include <SDL3/SDL_video.h>
#include <cassert>
#include <variant>
#include <vulkan/vulkan_enums.hpp>

namespace vulkan
{
	namespace
	{
		std::vector<vk::SurfaceFormatKHR> get_preferred_surface_formats(
			const SwapchainContext::Config& config
		) noexcept
		{
			switch (config.format)
			{
			case SwapchainFormat::SrgbUnorm8:
				return {
					{.format = vk::Format::eB8G8R8A8Srgb, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear},
					{.format = vk::Format::eR8G8B8A8Srgb, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear}
				};
			case SwapchainFormat::LinearUnorm8:
				return {
					{.format = vk::Format::eB8G8R8A8Unorm, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear},
					{.format = vk::Format::eR8G8B8A8Unorm, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear}
				};
			default:
				return {};
			}
		}

		std::optional<vk::SurfaceFormatKHR> select_surface_format(
			const vk::raii::PhysicalDevice& phy_device,
			const vk::SurfaceKHR& surface,
			const SwapchainContext::Config& config
		) noexcept
		{
			const auto available_formats = phy_device.getSurfaceFormatsKHR(surface);

			for (const auto& preferred_format : get_preferred_surface_formats(config))
			{
				auto found = std::ranges::find_if(
					available_formats,
					[preferred_format](const vk::SurfaceFormatKHR& avail) {
						return std::tie(avail.format, avail.colorSpace)
							== std::tie(preferred_format.format, preferred_format.colorSpace);
					}
				);
				if (found != available_formats.end()) return *found;
			}

			if (!available_formats.empty()) return std::nullopt;

			return std::nullopt;
		}

		std::vector<vk::PresentModeKHR> get_preferred_present_modes(bool vsync) noexcept
		{
			if (vsync)
			{
				return {
					vk::PresentModeKHR::eFifoRelaxed,
					vk::PresentModeKHR::eFifo,
				};
			}
			else
			{
				return {
					vk::PresentModeKHR::eMailbox,
					vk::PresentModeKHR::eImmediate,
				};
			}
		}

		vk::PresentModeKHR select_present_mode(
			const vk::raii::PhysicalDevice& phy_device,
			const vk::SurfaceKHR& surface,
			const SwapchainContext::Config& config
		) noexcept
		{
			const auto available_present_modes = phy_device.getSurfacePresentModesKHR(surface);
			const auto preferred_present_modes = get_preferred_present_modes(config.vsync);

			for (const auto& preferred_mode : preferred_present_modes)
				if (std::ranges::count(available_present_modes, preferred_mode) > 0) return preferred_mode;

			return vk::PresentModeKHR::eFifo;
		}

		std::expected<vk::raii::ImageView, Error> create_swapchain_imageview(
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
	}

	SwapchainContext::SwapchainContext(
		vk::SharingMode sharing_mode,
		std::vector<uint32_t> queue_family_indices,
		vk::SurfaceFormatKHR surface_format,
		vk::PresentModeKHR present_mode
	) noexcept :
		sharing_mode(sharing_mode),
		queue_family_indices(std::make_unique<const std::vector<uint32_t>>(std::move(queue_family_indices))),
		surface_format(surface_format),
		present_mode(present_mode)
	{}

	std::expected<SwapchainContext, Error> SwapchainContext::create(
		const SurfaceInstanceContext& instance_context,
		const SurfaceDeviceContext& device_context,
		const Config& config
	) noexcept
	{
		const auto& phy_device = device_context.get().phy_device;
		const auto surface = instance_context->surface;

		const auto format_result = select_surface_format(phy_device, surface, config);
		if (!format_result) return Error("Select surface format failed");
		const auto surface_format = *format_result;

		const auto present_mode = select_present_mode(phy_device, surface, config);

		const auto [sharing_mode, queue_family_indices] =
			[&]() -> std::pair<vk::SharingMode, std::vector<uint32_t>> {
			std::vector<uint32_t> queue_family_indices;
			if (device_context.get().family == device_context.get_present_queue().family_index)
			{
				queue_family_indices.push_back(device_context.get_present_queue().family_index);
				return std::make_pair(vk::SharingMode::eExclusive, std::move(queue_family_indices));
			}
			else
			{
				queue_family_indices.push_back(device_context.get().family);
				queue_family_indices.push_back(device_context.get_present_queue().family_index);
				return std::make_pair(vk::SharingMode::eConcurrent, std::move(queue_family_indices));
			}
		}();

		return SwapchainContext(sharing_mode, queue_family_indices, surface_format, present_mode);
	}

	std::expected<std::optional<SwapchainContext::Frame>, Error> SwapchainContext::acquire_next(
		const SurfaceInstanceContext& instance_context,
		const SurfaceDeviceContext& device_context,
		std::optional<vk::Semaphore> semaphore,
		std::optional<vk::Fence> fence,
		uint64_t timeout
	) noexcept
	{
		/* Check Minimized */

		const auto window_flags = SDL_GetWindowFlags(instance_context->window);
		if ((window_flags & SDL_WINDOW_MINIMIZED) != 0) return std::nullopt;

		/* Get Window Size */

		int window_width, window_height;
		if (!SDL_GetWindowSizeInPixels(instance_context->window, &window_width, &window_height))
			return Error("Get window size in pixels failed", SDL_GetError());
		assert(window_width >= 0 && window_height >= 0);
		const auto window_size = glm::u32vec2(window_width, window_height);

		/* Resize */

		if (std::holds_alternative<RuntimeState>(state))
		{
			auto& runtime_state = std::get<RuntimeState>(state);
			if (runtime_state.extent != window_size)
			{
				device_context->waitIdle();
				state = InvalidatedState{.old_swapchain = std::move(std::get<RuntimeState>(state).swapchain)};
				return std::nullopt;
			}
		}
		else if (auto result = recreate_swapchain(instance_context, device_context, window_size); !result)
			return result.error().forward("Recreate swapchain failed");

		/* Get Swapchain */

		auto& runtime_state = std::get<RuntimeState>(state);
		const auto [result, idx] =
			runtime_state.swapchain
				.acquireNextImage(timeout, semaphore.value_or(nullptr), fence.value_or(nullptr));

		switch (result)
		{
		case vk::Result::eSuccess:
			return Frame{
				.extent = runtime_state.extent,
				.extent_changed = std::exchange(runtime_state.extent_changed, false),
				.index = idx,
				.image = runtime_state.images.at(idx),
				.image_view = runtime_state.image_views.at(idx),
			};

		case vk::Result::eErrorOutOfDateKHR:
		case vk::Result::eSuboptimalKHR:
			device_context->waitIdle();
			state = InvalidatedState{.old_swapchain = std::move(runtime_state.swapchain)};
			return std::nullopt;

		default:
			return Error::from(result);
		}
	}

	std::expected<void, Error> SwapchainContext::present(
		const SurfaceDeviceContext& device_context,
		Frame frame,
		std::optional<vk::Semaphore> wait_semaphore
	) noexcept
	{
		if (!std::holds_alternative<RuntimeState>(state))
			return Error("Present failed", "Swapchain is not in a valid state");

		auto& runtime_state = std::get<RuntimeState>(state);

		std::vector<vk::Semaphore> wait_semaphores;
		if (wait_semaphore) wait_semaphores.push_back(*wait_semaphore);

		const auto swapchains = std::to_array({*runtime_state.swapchain});
		const auto image_indices = std::to_array({frame.index});

		const auto present_info =
			vk::PresentInfoKHR{}
				.setImageIndices(image_indices)
				.setWaitSemaphores(wait_semaphores)
				.setSwapchains(swapchains);

		const auto present_result = device_context.get_present_queue().queue->presentKHR(present_info);
		switch (present_result)
		{
		case vk::Result::eSuccess:
			return {};

		case vk::Result::eErrorOutOfDateKHR:
		case vk::Result::eSuboptimalKHR:
			device_context->waitIdle();
			state = InvalidatedState{.old_swapchain = std::move(runtime_state.swapchain)};
			return {};

		default:
			return Error::from(present_result);
		}
	}

	std::expected<void, Error> SwapchainContext::recreate_swapchain(
		const SurfaceInstanceContext& instance_context,
		const SurfaceDeviceContext& device_context,
		glm::u32vec2 extent
	) noexcept
	{
		const auto prev_swapchain =
			util::get_variant<InvalidatedState>(state)
				.transform([](InvalidatedState& invalid) { return std::move(invalid.old_swapchain); })
				.value_or(nullptr);

		const auto surface_capability =
			device_context.get().phy_device.getSurfaceCapabilitiesKHR(instance_context->surface);

		const auto image_count =
			glm::clamp<uint32_t>(3, surface_capability.minImageCount, surface_capability.maxImageCount);
		const auto min_extent =
			glm::u32vec2(surface_capability.minImageExtent.width, surface_capability.minImageExtent.height);
		const auto max_extent =
			glm::u32vec2(surface_capability.maxImageExtent.width, surface_capability.maxImageExtent.height);
		const auto clamped_extent = glm::clamp(extent, min_extent, max_extent);

		auto swapchain_create_info = vk::SwapchainCreateInfoKHR{
			.flags = {},
			.surface = instance_context->surface,
			.minImageCount = image_count,
			.imageFormat = surface_format.format,
			.imageColorSpace = surface_format.colorSpace,
			.imageExtent = vulkan::to<vk::Extent2D>(clamped_extent),
			.imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
			.imageSharingMode = sharing_mode,
			.preTransform = surface_capability.currentTransform,
			.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
			.presentMode = present_mode,
			.clipped = vk::True,
			.oldSwapchain = prev_swapchain
		};
		swapchain_create_info.setQueueFamilyIndices(*queue_family_indices);

		auto swapchain_result =
			device_context.get()
				.device.createSwapchainKHR(swapchain_create_info)
				.transform_error(Error::from<vk::Result>());
		if (!swapchain_result) return swapchain_result.error().forward("Create swapchain failed");
		auto swapchain = std::move(*swapchain_result);

		const auto images = swapchain.getImages();

		std::vector<vk::raii::ImageView> image_views;
		for (const auto& image : images)
		{
			auto image_view_result =
				create_swapchain_imageview(device_context.get().device, image, surface_format.format);
			if (!image_view_result)
				return image_view_result.error().forward("Create image view for swapchain image failed");
			auto image_view = std::move(*image_view_result);

			image_views.emplace_back(std::move(image_view));
		}

		state = RuntimeState{
			.swapchain = std::move(swapchain),
			.extent = clamped_extent,
			.images = images,
			.image_views = std::move(image_views)
		};

		return {};
	}

	SwapchainContext::ReadonlyWrapper SwapchainContext::operator->() const noexcept
	{
		return ReadonlyWrapper{
			.sharing_mode = sharing_mode,
			.queue_family_indices = std::span(*queue_family_indices),
			.surface_format = surface_format,
			.present_mode = present_mode
		};
	}
}
