#pragma once

#include "vulkan-window/swapchain.hpp"
#include "vulkan-window/vulkan.hpp"
#include "vulkan-window/window.hpp"

#include <limits>

///
/// @brief Vulkan Window
/// @details
///
/// ### Creation
///
/// 1. Fill in create info struct (`VulkanWindow::CreateInfo`)
/// 2. Call `VulkanWindow::create(create_info)` to create the context
///
/// ### Usage
///
/// 1. Process SDL events as usual and prepare data needed for rendering
///
/// 2. Call `VulkanWindow::acquire_swapchain_image(...)` to acquire the next swapchain image, where *Out Of
/// Date* or *Suboptimal* swapchains are automatically handled.. Generally, a semaphore should be provided
/// here to signal when the image is ready.
///   ```cpp
///   auto acquire_result = context.acquire_swapchain_image(image_ready_semaphore);
///   ```
///
/// 3. Record and submit command buffers to render to the acquired image. Generally, it should wait for
/// `image_ready_semaphore` and signal another semaphore (eg. `render_ready_semaphore`)
///
/// 4. Call `VulkanWindow::present_swapchain_image(...)` to present the rendered image. Similarly, wait for
/// `render_ready_semaphore` here. *Out Of Date* or *Suboptimal* swapchains are automatically handled.
///   ```cpp
///   auto present_result = context.present_swapchain_image(
///       acquire_result.image_index,
///       render_ready_semaphore
///   );
///   ```
///
class VulkanWindow
{
  public:

	struct CreateInfo
	{
		vulkan_window::WindowInfo window_info;
		vulkan_window::AppInfo app_info;
		vulkan_window::Features features;
	};

	std::unique_ptr<vulkan_window::WindowWrapper> window;

	vk::raii::Context context;
	vk::raii::Instance instance;
	std::unique_ptr<vulkan_window::SurfaceWrapper> surface;

	vk::raii::PhysicalDevice phy_device;
	vk::raii::Device device;
	vulkan_window::DeviceQueues queues;

	vulkan_window::SwapchainLayout swapchain_layout;

	///
	/// @brief Create a Vulkan context, with given creation info
	///
	/// @param create_info Create Info
	/// @return Successfully created context or Error
	///
	static std::expected<VulkanWindow, Error> create(const CreateInfo& create_info) noexcept;

	///
	/// @brief Acquire next image from the swapchain
	///
	/// @param signal_semaphore Semaphore to signal after a successful acquisition
	/// @param signal_fence Fence to signal after a successful acquisition
	/// @param timeout Acquisition timeout in nanoseconds
	/// @return Acquisition result or Error
	///
	std::expected<vulkan_window::SwapchainAcquireResult, Error> acquire_swapchain_image(
		std::optional<vk::Semaphore> signal_semaphore = std::nullopt,
		std::optional<vk::Fence> signal_fence = std::nullopt,
		uint64_t timeout = std::numeric_limits<uint64_t>::max()
	) noexcept;

	///
	/// @brief Present an image to the swapchain
	///
	/// @param image_index Index of the image to present
	/// @param wait_semaphore Semaphore to wait on before presenting
	/// @return Presentation result or Error
	///
	std::expected<void, Error> present_swapchain_image(
		uint32_t image_index,
		std::optional<vk::Semaphore> wait_semaphore = std::nullopt
	) noexcept;

  private:

	std::optional<vulkan_window::SwapchainInstance> swapchain = std::nullopt;

	VulkanWindow(
		std::unique_ptr<vulkan_window::WindowWrapper> window,
		vk::raii::Context context,
		vk::raii::Instance instance,
		std::unique_ptr<vulkan_window::SurfaceWrapper> surface,
		vk::raii::PhysicalDevice phy_device,
		vk::raii::Device device,
		vulkan_window::DeviceQueues queues,
		vulkan_window::SwapchainLayout swapchain_layout
	) :
		window(std::move(window)),
		context(std::move(context)),
		instance(std::move(instance)),
		surface(std::move(surface)),
		phy_device(std::move(phy_device)),
		device(std::move(device)),
		queues(std::move(queues)),
		swapchain_layout(std::move(swapchain_layout))
	{}

  public:

	VulkanWindow(const VulkanWindow&) = delete;
	VulkanWindow(VulkanWindow&&) = default;
	VulkanWindow& operator=(const VulkanWindow&) = delete;
	VulkanWindow& operator=(VulkanWindow&&) = default;
};