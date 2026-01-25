#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <cassert>
#include <expected>
#include <glm/glm.hpp>
#include <memory>

#include <common/util/error.hpp>

namespace vulkan
{
	///
	/// @brief Info for creating an SDL window
	///
	///
	struct WindowInfo
	{
		std::string title = "Vulkan Window";
		glm::u32vec2 initial_size = {800, 600};
		bool resizable = true;
		bool initial_fullscreen = false;

		SDL_WindowFlags get_flags() const noexcept;
	};

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

	namespace impl
	{
		std::expected<std::unique_ptr<WindowWrapper>, Error> create_window(
			const WindowInfo& window_info
		) noexcept;

		std::expected<std::span<const char* const>, Error> get_instance_extensions() noexcept;
	}
}