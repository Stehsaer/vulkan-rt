#include "vulkan-window/window.hpp"

#include <SDL3/SDL_vulkan.h>

namespace vulkan_window
{
	SDL_WindowFlags WindowInfo::get_flags() const noexcept
	{
		SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
		if (resizable) flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_RESIZABLE);
		if (initial_fullscreen) flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_FULLSCREEN);
		return flags;
	}

	WindowWrapper::~WindowWrapper() noexcept
	{
		SDL_DestroyWindow(window);
		SDL_Vulkan_UnloadLibrary();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	std::expected<std::span<const char* const>, Error> impl::get_instance_extensions() noexcept
	{
		unsigned int extension_count = 0;
		const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
		if (extensions == nullptr) return Error(Error::from_sdl, "Get instance extensions failed");

		return std::span(extensions, extension_count);
	}

	std::expected<std::unique_ptr<WindowWrapper>, Error> impl::create_window(
		const WindowInfo& window_info
	) noexcept
	{
		if (!SDL_Init(SDL_INIT_VIDEO)) return Error(Error::from_sdl, "Initialize SDL failed");

		const auto window = SDL_CreateWindow(
			window_info.title.c_str(),
			window_info.initial_size.x,
			window_info.initial_size.y,
			window_info.get_flags()
		);
		if (window == nullptr) return Error(Error::from_sdl, "Create SDL window failed");

		if (!SDL_Vulkan_LoadLibrary(nullptr)) return Error(Error::from_sdl, "Load Vulkan library failed");

		return std::make_unique<WindowWrapper>(window);
	}
}