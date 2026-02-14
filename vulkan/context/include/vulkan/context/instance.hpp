#pragma once

#include "common/util/error.hpp"
#include <SDL3/SDL_video.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	static constexpr uint32_t api_version = vk::ApiVersion14;

	///
	/// @brief Manages SDL window and Vulkan instance and surface
	/// @details
	/// - Call @p create to create an instance context. Customize options by modifying the @p Config struct
	/// - Use @p operator-> to access the Vulkan instance, SDL window and Vulkan surface
	///   ```cpp
	///   InstanceContext instance_context = ...;
	///   SDL_Window* window = instance_context->window;
	///   ```
	///
	class InstanceContext
	{
	  public:

		struct Config
		{
#ifdef NDEBUG
			static constexpr bool default_enable_validation = false;
#else
			static constexpr bool default_enable_validation = true;
#endif

			std::string title = "Vulkan Window";
			glm::u32vec2 initial_size = glm::u32vec2(800, 600);  // Initial window size in pixels

			bool resizable = true;
			bool initial_fullscreen = false;

			std::string application_name = "Vulkan Application";
			std::string engine_name = "No Engine";
			uint32_t application_version = VK_MAKE_VERSION(0, 0, 0);
			uint32_t engine_version = VK_MAKE_VERSION(0, 0, 0);

			bool validation = default_enable_validation;
		};

		///
		/// @brief Create an instance context with the given configuration
		///
		/// @param config Configuration
		/// @return Instance context or error
		///
		[[nodiscard]]
		static std::expected<InstanceContext, Error> create(const Config& config) noexcept;

	  private:

		class WindowWrapper
		{
			SDL_Window* window;

		  public:

			explicit WindowWrapper(SDL_Window* window_ptr) noexcept :
				window(window_ptr)
			{}

			~WindowWrapper() noexcept;

			[[nodiscard]]
			SDL_Window* get() const noexcept
			{
				return window;
			}
		};

		class SurfaceWrapper
		{
		  public:

			explicit SurfaceWrapper(vk::Instance instance, vk::SurfaceKHR surface) noexcept :
				instance(instance),
				surface(surface)
			{}

			~SurfaceWrapper() noexcept;

			[[nodiscard]]
			vk::SurfaceKHR get() const noexcept
			{
				return surface;
			}

		  private:

			vk::Instance instance;
			vk::SurfaceKHR surface;
		};

		struct ReadonlyWrapper
		{
			const vk::raii::Instance& instance;
			SDL_Window* window;
			vk::SurfaceKHR surface;

			const ReadonlyWrapper* operator->() const noexcept { return this; }
		};

		vk::raii::Context context;
		std::unique_ptr<WindowWrapper> window;
		std::unique_ptr<vk::raii::Instance> instance;
		std::unique_ptr<SurfaceWrapper> surface;

		explicit InstanceContext(
			vk::raii::Context context,
			std::unique_ptr<WindowWrapper> window,
			vk::raii::Instance instance,
			std::unique_ptr<SurfaceWrapper> surface
		) noexcept :
			context(std::move(context)),
			window(std::move(window)),
			instance(std::make_unique<vk::raii::Instance>(std::move(instance))),
			surface(std::move(surface))
		{}

	  public:

		ReadonlyWrapper operator->() const noexcept
		{
			return ReadonlyWrapper{.instance = *instance, .window = window->get(), .surface = surface->get()};
		}

		InstanceContext(const InstanceContext&) = delete;
		InstanceContext(InstanceContext&&) = default;
		InstanceContext& operator=(const InstanceContext&) = delete;
		InstanceContext& operator=(InstanceContext&&) = default;
	};
}