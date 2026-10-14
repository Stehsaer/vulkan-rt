#pragma once

#include "common/container/error.hpp"
#include "vulkan/common/context.hpp"

#include <SDL3/SDL_video.h>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief SDL window related config
	///
	struct WindowProperty
	{
		std::string title = "Vulkan Window";
		glm::u32vec2 initial_size = glm::u32vec2(800, 600);  // Initial window size in pixels

		bool resizable = true;
		bool initial_fullscreen = false;
	};

	///
	/// @brief Manages SDL window and Vulkan instance and surface
	///
	///
	class WindowedInstance
	{
	  public:

		///
		/// @brief Create a windowed instance context with the given configuration
		///
		/// @param property Instance property
		/// @param feature Instance feature
		/// @param window_property Window property
		/// @return Created windowed instance context or error
		///
		[[nodiscard]]
		static std::expected<WindowedInstance, Error> create(
			const InstanceProperty& property,
			const InstanceFeature& feature,
			const WindowProperty& window_property
		) noexcept;

	  private:

		struct SDLContextDestroyer
		{
			~SDLContextDestroyer();
		};

		struct WindowDeleter
		{
			void operator()(SDL_Window* ptr) const noexcept;
		};

		struct SurfaceDeleter
		{
			vk::Instance instance;

			void operator()(VkSurfaceKHR ptr) const noexcept;
		};

		std::unique_ptr<SDLContextDestroyer> context_destroyer;
		vk::raii::Context context;
		std::unique_ptr<SDL_Window, WindowDeleter> window;
		std::unique_ptr<vk::raii::Instance> instance;
		std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, SurfaceDeleter> surface;

		explicit WindowedInstance(
			std::unique_ptr<SDLContextDestroyer> context_destroyer,
			vk::raii::Context context,
			std::unique_ptr<SDL_Window, WindowDeleter> window,
			vk::raii::Instance instance,
			std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, SurfaceDeleter> surface
		) noexcept :
			context_destroyer(std::move(context_destroyer)),
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
			return VisitProxy{.instance = *instance, .window = window.get(), .surface = surface.get()};
		}

		WindowedInstance(const WindowedInstance&) = delete;
		WindowedInstance(WindowedInstance&&) = default;
		WindowedInstance& operator=(const WindowedInstance&) = delete;
		WindowedInstance& operator=(WindowedInstance&&) = default;
	};
}
