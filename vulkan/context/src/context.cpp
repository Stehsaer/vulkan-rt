#include "vulkan/context.hpp"
#include "impl.hpp"
#include "vulkan/context/swapchain.hpp"

namespace vulkan
{
	std::expected<Context, Error> Context::create(const CreateInfo& create_info) noexcept
	{
		/* Create Window */

		auto window_expected = context::impl::create_window(create_info.window_info);
		if (!window_expected) return window_expected.error().forward("Create window failed");
		auto window = std::move(*window_expected);

		/* Create Vulkan Context */

		auto context = vk::raii::Context{};

		/* Create Vulkan Instance */

		auto instance_expected =
			context::impl::create_instance(context, create_info.app_info, create_info.features);
		if (!instance_expected) return instance_expected.error().forward("Create vulkan instance failed");
		auto instance = std::move(*instance_expected);

		/* Create Surface */

		auto surface_expected = context::impl::create_surface(instance, *window);
		if (!surface_expected) return surface_expected.error().forward("Create surface failed");
		auto surface = std::move(*surface_expected);

		/* Pick Physical Device */

		auto phy_device_expected = context::impl::pick_physical_device(instance, create_info.features);
		if (!phy_device_expected) return phy_device_expected.error().forward("Pick physical device failed");
		auto phy_device = std::move(*phy_device_expected);

		/* Create Logical Device and Queues */

		auto device_expected =
			context::impl::create_logical_device(phy_device, vk::SurfaceKHR(*surface), create_info.features);
		if (!device_expected) return device_expected.error().forward("Create logical device failed");
		auto [device, queues] = std::move(*device_expected);

		/* Select Swapchain Layout */

		auto swapchain_layout_expected =
			context::impl::select_swapchain_layout(phy_device, vk::SurfaceKHR(*surface), queues);
		if (!swapchain_layout_expected)
			return swapchain_layout_expected.error().forward("Select swapchain layout failed");
		auto swapchain_layout = std::move(*swapchain_layout_expected);

		/* Create Memory Allocator */

		auto allocator_expected = vulkan::alloc::Allocator::create(instance, phy_device, device);
		if (!allocator_expected) return allocator_expected.error().forward("Create allocator failed");
		auto allocator = std::move(*allocator_expected);

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
				auto swapchain_expected = vulkan::SwapchainInstance::create(
					phy_device,
					device,
					vk::SurfaceKHR(*surface),
					swapchain_layout,
					std::move(swapchain)
				);
				if (!swapchain_expected)
					return swapchain_expected.error().forward("Recreate swapchain failed");
				swapchain = std::move(*swapchain_expected);

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
					auto swapchain_expected = vulkan::SwapchainInstance::create(
						phy_device,
						device,
						vk::SurfaceKHR(*surface),
						swapchain_layout,
						std::move(swapchain)
					);
					if (!swapchain_expected)
						return swapchain_expected.error().forward("Recreate swapchain failed");
					swapchain = std::move(*swapchain_expected);

					continue;
				}
				case vk::Result::eNotReady:
					continue;
				default:
					return Error(acquire_expected.error(), "Acquire swapchain image failed");
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
			auto swapchain_expected = vulkan::SwapchainInstance::create(
				phy_device,
				device,
				vk::SurfaceKHR(*surface),
				swapchain_layout,
				std::move(swapchain)
			);
			if (!swapchain_expected) return swapchain_expected.error().forward("Recreate swapchain failed");
			swapchain = std::move(*swapchain_expected);
			return {};
		}
		default:
			return Error(present_result, "Present swapchain image failed");
		}
	}
}