#pragma once

#include "vulkan/alloc.hpp"

#include <SDL3/SDL_video.h>
#include <glm/glm.hpp>
#include <limits>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{

#pragma region Initialization Structs

	///
	/// @brief Info for creating an SDL window
	///
	///
	struct WindowInfo
	{
		std::string title = "Vulkan Window";     // Window title
		glm::u32vec2 initial_size = {800, 600};  // Initial window size in pixels
		bool resizable = true;                   // Whether the window is resizable
		bool initial_fullscreen = false;         // Whether the window starts in fullscreen mode
	};

	///
	/// @brief Basic application info for Vulkan instance creation
	///
	///
	struct AppInfo
	{
		std::string application_name = "Vulkan Application";
		uint32_t application_version = VK_MAKE_VERSION(0, 0, 0);
		std::string engine_name = "No Engine";
		uint32_t engine_version = VK_MAKE_VERSION(0, 0, 0);
	};

	///
	/// @brief Features to enable in the Vulkan context
	///
	///
	struct Features
	{
		bool validation = true;  // Validation Layers
	};

#pragma endregion

	///
	/// @brief Queue families and queues
	/// @note Graphics and compute queues implicitly support transfer operations
	///
	struct DeviceQueues
	{
		std::shared_ptr<vk::raii::Queue> graphics;
		std::shared_ptr<vk::raii::Queue> compute;
		std::shared_ptr<vk::raii::Queue> present;

		uint32_t graphics_index;
		uint32_t compute_index;
		uint32_t present_index;
	};

#pragma region Windows & Surfaces

	///
	/// @brief Wrapper for auto destructing an SDL window
	///
	///
	class WindowWrapper
	{
		SDL_Window* window;

	  public:

		explicit WindowWrapper(SDL_Window* window_ptr) :
			window(window_ptr)
		{
			assert(window != nullptr);
		}

		~WindowWrapper() noexcept;

		operator SDL_Window*() const noexcept { return window; }
	};

	///
	/// @brief Wrapper for automatically destroying a Vulkan surface
	///
	///
	class SurfaceWrapper
	{
	  public:

		SurfaceWrapper(vk::Instance instance, vk::SurfaceKHR surface) :
			instance(instance),
			surface(surface)
		{}

		~SurfaceWrapper() noexcept;

		operator vk::SurfaceKHR() const noexcept { return surface; }

	  private:

		vk::Instance instance;
		vk::SurfaceKHR surface;
	};

#pragma endregion

#pragma region Swapchain

	///
	/// @brief Stores selected swapchain layout details
	///
	///
	struct SwapchainLayout
	{
		vk::SharingMode image_sharing_mode;
		std::vector<uint32_t> image_queue_family_indices;
		vk::SurfaceFormatKHR surface_format;
		vk::PresentModeKHR present_mode;
	};

	// A swapchain image and its view
	struct SwapchainImage
	{
		vk::Image image;
		vk::raii::ImageView view;
	};

	// Result of acquiring a swapchain image
	struct SwapchainAcquireResult
	{
		glm::u32vec2 extent;
		uint32_t image_index;
		const SwapchainImage& image;
	};

	///
	/// @brief Vulkan swapchain and its images, may be recreated as needed
	/// @note This class serves for internal usage, for acquiring swapchain images, see `Context`
	///
	class SwapchainInstance
	{
	  public:

		///
		/// @brief Create swapchain for the given window and vulkan context
		///
		/// @param window Window context
		/// @param vulkan Vulkan context
		/// @param old_swapchain Optional old swapchain for recreation
		/// @return New Swapchain instance or Error
		///
		[[nodiscard]]
		static std::expected<SwapchainInstance, Error> create(
			const vk::raii::PhysicalDevice& phy_device,
			const vk::raii::Device& device,
			vk::SurfaceKHR surface,
			const SwapchainLayout& swapchain_layout,
			std::optional<SwapchainInstance> old_swapchain
		) noexcept;

		///
		/// @brief Acquire the next image from the swapchain
		///
		/// @param semaphore Semaphore object, can be empty
		/// @param fence Fence object, can be empty
		/// @param timeout Timeout, default to UINT64_MAX
		/// @return Acquired image or Vulkan warning/error
		///
		[[nodiscard]]
		std::expected<SwapchainAcquireResult, vk::Result> acquire_next_image(
			std::optional<vk::Semaphore> semaphore = std::nullopt,
			std::optional<vk::Fence> fence = std::nullopt,
			uint64_t timeout = std::numeric_limits<uint64_t>::max()
		) noexcept;

		///
		/// @brief Get the current swapchain image
		///
		/// @param index Index of the image to get
		/// @return Reference to the swapchain image
		///
		[[nodiscard]]
		const SwapchainImage& operator[](size_t index) const noexcept
		{
			return images.at(index);
		}

		[[nodiscard]]
		vk::SwapchainKHR get_swapchain() const noexcept
		{
			return swapchain;
		}

		SwapchainInstance(const SwapchainInstance&) = delete;
		SwapchainInstance(SwapchainInstance&&) noexcept = default;
		SwapchainInstance& operator=(const SwapchainInstance&) = delete;
		SwapchainInstance& operator=(SwapchainInstance&&) = default;

	  private:

		std::vector<SwapchainImage> images;
		vk::raii::SwapchainKHR swapchain;
		glm::u32vec2 extent;

		SwapchainInstance(
			std::vector<SwapchainImage> images,
			vk::raii::SwapchainKHR swapchain,
			glm::u32vec2 extent
		) :
			images(std::move(images)),
			swapchain(std::move(swapchain)),
			extent(extent)
		{}
	};

#pragma endregion

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
	///    - *Out Of Date* or *Suboptimal* swapchains are automatically handled. In such cases, it will block
	///    the thread until a valid swapchain is created.
	///    - Generally, a semaphore is provided here to signal when the image is ready.
	///
	/// ##### Example
	///   ```cpp
	///   auto acquire_result = context.acquire_swapchain_image(image_ready_semaphore);
	///   ```
	///
	/// 3. **Record and submit command buffers to render to the acquired image.**
	///     - Generally, it waits for `image_ready_semaphore` and signals another semaphore (eg.
	///     `render_ready_semaphore`).
	///
	/// 4. **Call `Context::present_swapchain_image(...)` to present the rendered image.**
	///     - Similarly, wait for `render_ready_semaphore` here.
	///     - *Out Of Date* or *Suboptimal* swapchains are automatically handled.
	///     - Remember to have the swapchain layout set to present.
	///
	/// ##### Example
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
		[[nodiscard]]
		static std::expected<Context, Error> create(const CreateInfo& create_info) noexcept;

		///
		/// @brief Acquire next image from the swapchain
		///
		/// @param signal_semaphore Semaphore to signal after a successful acquisition
		/// @param signal_fence Fence to signal after a successful acquisition
		/// @param timeout Acquisition timeout in nanoseconds
		/// @return Acquisition result or Error
		///
		[[nodiscard]]
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
		[[nodiscard]]
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