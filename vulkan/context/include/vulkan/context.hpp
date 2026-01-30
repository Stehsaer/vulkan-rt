#pragma once

#include "context/swapchain.hpp"
#include "context/vulkan.hpp"
#include "context/window.hpp"
#include "vulkan/alloc.hpp"

#include <limits>

namespace vulkan
{
	///
	/// @brief Vulkan Context
	/// @details
	///
	/// ### Creation
	///
	/// 1. Fill in create info struct (`Context::CreateInfo`)
	/// 2. Call `Context::create(create_info)` to create the context
	///
	/// ### Usage
	///
	/// 1. **Process SDL events as usual and prepare data needed for rendering**
	///
	/// 2. **Call `Context::acquire_swapchain_image(...)` to acquire the next swapchain image.**
	///    - *Out Of Date* or *Suboptimal* swapchains are automatically handled.
	///    - Generally, a semaphore should be provided here to signal when the image is ready.
	///   ```cpp
	///   auto acquire_result = context.acquire_swapchain_image(image_ready_semaphore);
	///   ```
	///
	/// 3. **Record and submit command buffers to render to the acquired image.**
	///     - Generally, it should wait for `image_ready_semaphore` and signal another semaphore (eg.
	///     `render_ready_semaphore`).
	///
	/// 4. **Call `Context::present_swapchain_image(...)` to present the rendered image.**
	///     - Similarly, wait for `render_ready_semaphore` here.
	///     - *Out Of Date* or *Suboptimal* swapchains are automatically handled.
	///     - Remember to have the swapchain layout set to present.
	///   ```cpp
	///   auto present_result = context.present_swapchain_image(
	///       acquire_result.image_index,
	///       render_ready_semaphore
	///   );
	///   ```
	///
	class Context
	{
	  public:

		struct CreateInfo
		{
			vulkan::WindowInfo window_info;
			vulkan::AppInfo app_info;
			vulkan::Features features;
		};

		std::unique_ptr<vulkan::WindowWrapper> window;

		vk::raii::Context context;
		vk::raii::Instance instance;
		std::unique_ptr<vulkan::SurfaceWrapper> surface;

		vk::raii::PhysicalDevice phy_device;
		vk::raii::Device device;
		vulkan::DeviceQueues queues;

		vulkan::SwapchainLayout swapchain_layout;

		vulkan::alloc::Allocator allocator;

		///
		/// @brief Create a Vulkan context, with given creation info
		///
		/// @param create_info Create Info
		/// @return Successfully created context or Error
		///
		static std::expected<Context, Error> create(const CreateInfo& create_info) noexcept;

		///
		/// @brief Acquire next image from the swapchain
		///
		/// @param signal_semaphore Semaphore to signal after a successful acquisition
		/// @param signal_fence Fence to signal after a successful acquisition
		/// @param timeout Acquisition timeout in nanoseconds
		/// @return Acquisition result or Error
		///
		std::expected<vulkan::SwapchainAcquireResult, Error> acquire_swapchain_image(
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

		std::optional<vulkan::SwapchainInstance> swapchain = std::nullopt;

		Context(
			std::unique_ptr<vulkan::WindowWrapper> window,
			vk::raii::Context context,
			vk::raii::Instance instance,
			std::unique_ptr<vulkan::SurfaceWrapper> surface,
			vk::raii::PhysicalDevice phy_device,
			vk::raii::Device device,
			vulkan::DeviceQueues queues,
			vulkan::SwapchainLayout swapchain_layout,
			vulkan::alloc::Allocator allocator
		) :
			window(std::move(window)),
			context(std::move(context)),
			instance(std::move(instance)),
			surface(std::move(surface)),
			phy_device(std::move(phy_device)),
			device(std::move(device)),
			queues(std::move(queues)),
			swapchain_layout(std::move(swapchain_layout)),
			allocator(std::move(allocator))
		{}

	  public:

		Context(const Context&) = delete;
		Context(Context&&) = default;
		Context& operator=(const Context&) = delete;
		Context& operator=(Context&&) = default;
	};
}