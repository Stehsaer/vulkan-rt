#include "vulkan/context/swapchain.hpp"
#include "common/util/variant.hpp"

namespace vulkan
{
	namespace
	{
		std::optional<vk::SurfaceFormatKHR> select_surface_format(
			const vk::raii::PhysicalDevice& phy_device,
			const vk::SurfaceKHR& surface
		) noexcept
		{
			const auto available_formats = phy_device.getSurfaceFormatsKHR(surface);
			const auto preferred_formats = std::to_array<vk::SurfaceFormatKHR>({
				{.format = vk::Format::eB8G8R8A8Srgb, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear},
				{.format = vk::Format::eR8G8B8A8Srgb, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear}
			});

			for (const auto& preferred_format : preferred_formats)
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

			if (!available_formats.empty()) return available_formats.front();

			return std::nullopt;
		}

		vk::PresentModeKHR select_present_mode(
			const vk::raii::PhysicalDevice& phy_device,
			const vk::SurfaceKHR& surface
		) noexcept
		{
			const auto available_present_modes = phy_device.getSurfacePresentModesKHR(surface);
			constexpr auto preferred_present_modes =
				std::to_array({vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eFifoRelaxed});

			for (const auto& preferred_mode : preferred_present_modes)
				if (std::ranges::find(available_present_modes, preferred_mode)
					!= available_present_modes.end())
					return preferred_mode;

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
		const InstanceContext& instance_context,
		const DeviceContext& device_context
	) noexcept
	{
		const auto& phy_device = device_context.phy_device;
		const auto surface = instance_context->surface;

		const auto format_result = select_surface_format(phy_device, surface);
		if (!format_result) return Error("Select surface format failed");
		const auto surface_format = *format_result;

		const auto present_mode = select_present_mode(phy_device, surface);

		const auto [sharing_mode, queue_family_indices] =
			[&]() -> std::pair<vk::SharingMode, std::vector<uint32_t>> {
			std::vector<uint32_t> queue_family_indices;
			if (device_context.graphics_queue.family_index == device_context.present_queue.family_index)
			{
				queue_family_indices.push_back(device_context.graphics_queue.family_index);
				return std::make_pair(vk::SharingMode::eExclusive, std::move(queue_family_indices));
			}
			else
			{
				queue_family_indices.push_back(device_context.graphics_queue.family_index);
				queue_family_indices.push_back(device_context.present_queue.family_index);
				return std::make_pair(vk::SharingMode::eConcurrent, std::move(queue_family_indices));
			}
		}();

		return SwapchainContext(sharing_mode, queue_family_indices, surface_format, present_mode);
	}

	std::expected<SwapchainContext::Frame, Error> SwapchainContext::acquire_next(
		const InstanceContext& instance_context,
		const DeviceContext& device_context,
		std::optional<vk::Semaphore> semaphore,
		std::optional<vk::Fence> fence,
		uint64_t timeout
	) noexcept
	{
		while (true)
		{
			auto pre_acquire_recreate_result = check_and_recreate_swapchain(instance_context, device_context);
			if (!pre_acquire_recreate_result)
				return pre_acquire_recreate_result.error().forward("Recreate swapchain failed");

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
					.image_view = runtime_state.image_views.at(idx)
				};

			case vk::Result::eErrorOutOfDateKHR:
			case vk::Result::eSuboptimalKHR:
				state = InvalidatedState{.old_swapchain = std::move(runtime_state.swapchain)};
				continue;

			default:
				return Error::from(result);
			}
		}
	}

	std::expected<void, Error> SwapchainContext::present(
		const DeviceContext& device_context,
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

		const auto present_result = device_context.present_queue.queue->presentKHR(present_info);
		switch (present_result)
		{
		case vk::Result::eSuccess:
			return {};

		case vk::Result::eErrorOutOfDateKHR:
		case vk::Result::eSuboptimalKHR:
			state = InvalidatedState{.old_swapchain = std::move(runtime_state.swapchain)};
			return {};

		default:
			return Error::from(present_result);
		}
	}

	std::expected<void, Error> SwapchainContext::check_and_recreate_swapchain(
		const InstanceContext& instance_context,
		const DeviceContext& device_context
	) noexcept
	{
		if (std::holds_alternative<RuntimeState>(state)) return {};

		const auto prev_swapchain =
			util::get_variant<InvalidatedState>(state)
				.transform([](InvalidatedState& invalid) { return std::move(invalid.old_swapchain); })
				.value_or(nullptr);

		const auto surface_capability =
			device_context.phy_device.getSurfaceCapabilitiesKHR(instance_context->surface);

		const auto image_count =
			glm::clamp<uint32_t>(3, surface_capability.minImageCount, surface_capability.maxImageCount);

		auto swapchain_create_info = vk::SwapchainCreateInfoKHR{
			.flags = {},
			.surface = instance_context->surface,
			.minImageCount = image_count,
			.imageFormat = surface_format.format,
			.imageColorSpace = surface_format.colorSpace,
			.imageExtent = surface_capability.currentExtent,
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
			device_context.device.createSwapchainKHR(swapchain_create_info)
				.transform_error(Error::from<vk::Result>());
		if (!swapchain_result) return swapchain_result.error().forward("Create swapchain failed");
		auto swapchain = std::move(*swapchain_result);

		const auto images = swapchain.getImages();

		std::vector<vk::raii::ImageView> image_views;
		for (const auto& image : images)
		{
			auto image_view_result =
				create_swapchain_imageview(device_context.device, image, surface_format.format);
			if (!image_view_result)
				return image_view_result.error().forward("Create image view for swapchain image failed");
			auto image_view = std::move(*image_view_result);

			image_views.emplace_back(std::move(image_view));
		}

		state = RuntimeState{
			.swapchain = std::move(swapchain),
			.extent =
				glm::u32vec2(surface_capability.currentExtent.width, surface_capability.currentExtent.height),
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