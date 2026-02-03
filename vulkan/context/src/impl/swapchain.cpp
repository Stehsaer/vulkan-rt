#include "impl/swapchain.hpp"

namespace vulkan::context
{
	namespace
	{
		std::optional<vk::SurfaceFormatKHR> select_surface_format(
			const vk::raii::PhysicalDevice& phy_device,
			const VkSurfaceKHR& surface
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
			const VkSurfaceKHR& surface
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
	}

	std::expected<SwapchainLayout, Error> select_swapchain_layout(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		const DeviceQueues& queues
	) noexcept
	{
		/* Select Sharing Mode */

		vk::SharingMode image_sharing_mode;
		std::vector<uint32_t> image_queue_family_indices;

		if (queues.graphics_index != queues.present_index)
		{
			image_sharing_mode = vk::SharingMode::eConcurrent;
			image_queue_family_indices = {queues.graphics_index, queues.present_index};
		}
		else
		{
			image_sharing_mode = vk::SharingMode::eExclusive;
		}

		/* Find Format */

		auto surface_format_opt = select_surface_format(phy_device, surface);
		if (!surface_format_opt) return Error("No supported surface format found for swapchain");

		/* Select Present Mode */

		const auto present_mode = select_present_mode(phy_device, surface);

		return SwapchainLayout{
			.image_sharing_mode = image_sharing_mode,
			.image_queue_family_indices = image_queue_family_indices,
			.surface_format = *surface_format_opt,
			.present_mode = present_mode
		};
	}
}