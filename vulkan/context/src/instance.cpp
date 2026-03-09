#include "vulkan/context/instance.hpp"
#include "impl/instance.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <string>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	SDLContextDestroyer::~SDLContextDestroyer()
	{
		SDL_Vulkan_UnloadLibrary();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	std::expected<HeadlessInstanceContext, Error> HeadlessInstanceContext::create(
		const InstanceConfig& instance_config
	) noexcept
	{
		/* Step 1: Context Initialization */

		auto context_result = impl::init_sdl();
		if (!context_result) return context_result.error().forward("Initialize context failed");
		auto context = std::move(*context_result);

		/* Step 2: Instance */

		auto instance_result = impl::create_instance_headless(context, instance_config);
		if (!instance_result) return instance_result.error().forward("Create Vulkan instance failed");
		auto instance = std::move(*instance_result);

		return HeadlessInstanceContext(std::move(context), std::move(instance));
	}

	std::expected<SurfaceInstanceContext, Error> SurfaceInstanceContext::create(
		const WindowConfig& window_config,
		const InstanceConfig& instance_config
	) noexcept
	{
		/* Step 1: Context Initialization */

		auto context_result = impl::init_sdl();
		if (!context_result) return context_result.error().forward("Initialize context failed");
		auto context = std::move(*context_result);

		/* Step 2: Create SDL window */

		auto window_ptr_result = impl::create_window(window_config);
		if (!window_ptr_result) return window_ptr_result.error().forward("Create window failed");
		auto window = std::make_unique<Window>(*window_ptr_result);

		/* Step 3: Instance */

		auto instance_result = impl::create_instance_surface(context, instance_config, window_config);
		if (!instance_result) return instance_result.error().forward("Create Vulkan instance failed");
		auto instance = std::move(*instance_result);

		/* Step 4: Surface */

		VkSurfaceKHR surface_raw;
		if (!SDL_Vulkan_CreateSurface(window->get(), *instance, nullptr, &surface_raw))
			return Error("Create surface for SDL window failed", SDL_GetError());
		auto surface = std::make_unique<Surface>(*instance, surface_raw);

		return SurfaceInstanceContext(
			std::move(context),
			std::move(window),
			std::move(instance),
			std::move(surface)
		);
	}

	SurfaceInstanceContext::Surface::~Surface() noexcept
	{
		SDL_Vulkan_DestroySurface(instance, surface, nullptr);
	}

	SurfaceInstanceContext::Window::~Window() noexcept
	{
		SDL_DestroyWindow(window);
	}
}
