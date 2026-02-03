#include "impl/window.hpp"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>

namespace vulkan::context
{
	static SDL_WindowFlags get_flags(const WindowInfo& window_info) noexcept
	{
		SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
		if (window_info.resizable) flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_RESIZABLE);
		if (window_info.initial_fullscreen)
			flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_FULLSCREEN);
		return flags;
	}

	std::expected<std::unique_ptr<WindowWrapper>, Error> create_window(const WindowInfo& window_info) noexcept
	{
		if (!SDL_Init(SDL_INIT_VIDEO)) return Error(Error::from_sdl, "Initialize SDL failed");

		const auto window = SDL_CreateWindow(
			window_info.title.c_str(),
			window_info.initial_size.x,
			window_info.initial_size.y,
			get_flags(window_info)
		);
		if (window == nullptr) return Error(Error::from_sdl, "Create SDL window failed");

		if (!SDL_Vulkan_LoadLibrary(nullptr)) return Error(Error::from_sdl, "Load Vulkan library failed");

		return std::make_unique<WindowWrapper>(window);
	}

	std::expected<std::unique_ptr<SurfaceWrapper>, Error> create_surface(
		const vk::raii::Instance& instance,
		SDL_Window* window
	) noexcept
	{
		VkSurfaceKHR surface;
		if (!SDL_Vulkan_CreateSurface(window, *instance, nullptr, &surface))
			return Error("Create surface for SDL window failed");
		return std::make_unique<SurfaceWrapper>(instance, surface);
	}
}