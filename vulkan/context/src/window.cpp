#include "vulkan/context/window.hpp"

#include <SDL3/SDL_vulkan.h>

namespace vulkan
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
}