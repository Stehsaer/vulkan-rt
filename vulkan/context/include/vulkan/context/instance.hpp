#pragma once

#include "common/util/error.hpp"

#include <SDL3/SDL_video.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	static constexpr uint32_t API_VERSION = vk::ApiVersion14;

	///
	/// @brief Instance related config
	/// @note Some layers and extensions are not controlled by this config
	///
	struct InstanceConfig
	{
#ifdef NDEBUG
		static constexpr bool DEFAULT_ENABLE_VALIDATION = false;
#else
		static constexpr bool default_enable_validation = true;
#endif

		std::string application_name = "Vulkan Application";
		std::string engine_name = "No Engine";
		uint32_t application_version = VK_MAKE_VERSION(0, 0, 0);
		uint32_t engine_version = VK_MAKE_VERSION(0, 0, 0);

		bool validation = DEFAULT_ENABLE_VALIDATION;
	};

	///
	/// @brief SDL window related config
	///
	///
	struct WindowConfig
	{
		std::string title = "Vulkan Window";
		glm::u32vec2 initial_size = glm::u32vec2(800, 600);  // Initial window size in pixels

		bool resizable = true;
		bool initial_fullscreen = false;
	};

	///
	/// @brief Helper class for destroying SDL context upon destruction
	///
	///
	struct SDLContextDestroyer
	{
		~SDLContextDestroyer();
	};

	///
	/// @brief Vulkan context and instance, designed to work with window-less (headless) scenario
	///
	///
	class HeadlessInstanceContext
	{
	  public:

		///
		/// @brief Create a headless instance context
		///
		/// @param instance_config Instance config
		/// @return Create headless instance context, or error
		///
		[[nodiscard]]
		static std::expected<HeadlessInstanceContext, Error> create(
			const InstanceConfig& instance_config
		) noexcept;

	  private:

		std::unique_ptr<SDLContextDestroyer> context_destroyer;
		vk::raii::Context context;
		std::unique_ptr<vk::raii::Instance> instance;

		explicit HeadlessInstanceContext(vk::raii::Context context, vk::raii::Instance instance) :
			context_destroyer(std::make_unique<SDLContextDestroyer>()),
			context(std::move(context)),
			instance(std::make_unique<vk::raii::Instance>(std::move(instance)))
		{}

		struct VisitProxy
		{
			const vk::raii::Instance& instance;

			const VisitProxy* operator->() const noexcept { return this; }
		};

	  public:

		VisitProxy operator->() const noexcept { return {.instance = *instance}; }

		HeadlessInstanceContext(const HeadlessInstanceContext&) = delete;
		HeadlessInstanceContext(HeadlessInstanceContext&&) = default;
		HeadlessInstanceContext& operator=(const HeadlessInstanceContext&) = delete;
		HeadlessInstanceContext& operator=(HeadlessInstanceContext&&) = default;
	};

	///
	/// @brief Manages SDL window and Vulkan instance and surface
	///
	///
	class SurfaceInstanceContext
	{
	  public:

		///
		/// @brief Create a surface instance context with the given configuration
		///
		/// @param window_config SDL window config
		/// @param instance_config Instance config
		/// @return Created surface instance context or error
		///
		[[nodiscard]]
		static std::expected<SurfaceInstanceContext, Error> create(
			const WindowConfig& window_config,
			const InstanceConfig& instance_config
		) noexcept;

	  private:

		class Window
		{
			SDL_Window* window;

		  public:

			explicit Window(SDL_Window* window_ptr) noexcept :
				window(window_ptr)
			{}

			~Window() noexcept;

			[[nodiscard]]
			SDL_Window* get() const noexcept
			{
				return window;
			}
		};

		class Surface
		{
		  public:

			explicit Surface(vk::Instance instance, vk::SurfaceKHR surface) noexcept :
				instance(instance),
				surface(surface)
			{}

			~Surface() noexcept;

			[[nodiscard]]
			vk::SurfaceKHR get() const noexcept
			{
				return surface;
			}

		  private:

			vk::Instance instance;
			vk::SurfaceKHR surface;
		};

		std::unique_ptr<SDLContextDestroyer> context_destroyer;
		vk::raii::Context context;
		std::unique_ptr<Window> window;
		std::unique_ptr<vk::raii::Instance> instance;
		std::unique_ptr<Surface> surface;

		explicit SurfaceInstanceContext(
			vk::raii::Context context,
			std::unique_ptr<Window> window,
			vk::raii::Instance instance,
			std::unique_ptr<Surface> surface
		) noexcept :
			context_destroyer(std::make_unique<SDLContextDestroyer>()),
			context(std::move(context)),
			window(std::move(window)),
			instance(std::make_unique<vk::raii::Instance>(std::move(instance))),
			surface(std::move(surface))
		{}

		struct VisitProxy
		{
			const vk::raii::Instance& instance;
			SDL_Window* window;
			vk::SurfaceKHR surface;

			const VisitProxy* operator->() const noexcept { return this; }
		};

	  public:

		VisitProxy operator->() const noexcept
		{
			return VisitProxy{.instance = *instance, .window = window->get(), .surface = surface->get()};
		}

		SurfaceInstanceContext(const SurfaceInstanceContext&) = delete;
		SurfaceInstanceContext(SurfaceInstanceContext&&) = default;
		SurfaceInstanceContext& operator=(const SurfaceInstanceContext&) = delete;
		SurfaceInstanceContext& operator=(SurfaceInstanceContext&&) = default;
	};
}
