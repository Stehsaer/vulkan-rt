#include "vulkan/platform/windowed/instance.hpp"
#include "common/container/error.hpp"
#include "common/util/set.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/platform/common/instance.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <memory>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	namespace
	{
		std::expected<vk::raii::Context, Error> init_sdl() noexcept
		{
#ifdef __linux__
			SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
#endif

			if (!SDL_Init(SDL_INIT_VIDEO)) return Error("Initialize SDL failed", SDL_GetError());

			const char* current_video_driver = SDL_GetCurrentVideoDriver();
			if (current_video_driver == nullptr)
				return Error("Get current video driver from SDL failed", SDL_GetError());

			if (!SDL_Vulkan_LoadLibrary(nullptr)) return Error("Load vulkan library failed", SDL_GetError());

			const auto dyn_func_ptr = SDL_Vulkan_GetVkGetInstanceProcAddr();
			if (dyn_func_ptr == nullptr) return Error("Get invalid pointer for vulkan library");

			return vk::raii::Context(reinterpret_cast<PFN_vkGetInstanceProcAddr>(dyn_func_ptr));
		};

		SDL_WindowFlags get_flags_sdl(const WindowProperty& config) noexcept
		{
			SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
			if (config.resizable) flags |= SDL_WINDOW_RESIZABLE;
			if (config.initial_fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
			return flags;
		}

		std::expected<std::set<std::string>, Error> get_instance_layers_sdl(
			const WindowProperty& property [[maybe_unused]]
		) noexcept
		{
			// Empty for now
			return std::set<std::string>();
		}

		std::expected<std::set<std::string>, Error> get_instance_extensions_sdl(
			const WindowProperty& property [[maybe_unused]]
		) noexcept
		{
			unsigned int extension_count = 0;
			const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
			if (extensions == nullptr) return Error("Get instance extensions failed", SDL_GetError());

			return std::set<std::string>(std::from_range, std::span(extensions, extension_count));
		}

		std::expected<SDL_Window*, Error> create_window(const WindowProperty& property) noexcept
		{
			const auto window_ptr = SDL_CreateWindow(
				property.title.c_str(),
				property.initial_size.x,
				property.initial_size.y,
				get_flags_sdl(property)
			);
			if (window_ptr == nullptr) return Error("Create SDL window failed", SDL_GetError());

			return window_ptr;
		}

		std::expected<vk::raii::Instance, Error> create_windowed_instance(
			const vk::raii::Context& context,
			const InstanceProperty& instance_property,
			const InstanceFeature& instance_feature,
			const WindowProperty& window_property
		) noexcept
		{
			using namespace util::set_operator;

			const auto instance_layers = platform::get_instance_layers(instance_feature);
			const auto instance_extensions = platform::get_instance_extensions(instance_feature);

			const auto instance_layers_sdl_result = get_instance_layers_sdl(window_property);
			const auto instance_extensions_sdl_result = get_instance_extensions_sdl(window_property);

			if (!instance_layers_sdl_result)
				return instance_layers_sdl_result.error().forward("Get instance layers from SDL failed");
			if (!instance_extensions_sdl_result)
				return instance_extensions_sdl_result.error().forward(
					"Get instance extensions from SDL failed"
				);

			const auto layers = instance_layers + *instance_layers_sdl_result;
			const auto extensions = instance_extensions + *instance_extensions_sdl_result;

			return platform::create_instance(context, instance_property, layers, extensions);
		}
	}

	std::expected<WindowedInstance, Error> WindowedInstance::create(
		const InstanceProperty& property,
		const InstanceFeature& feature,
		const WindowProperty& window_property
	) noexcept
	{
		/* Step 1: Context Initialization */

		auto context_result = init_sdl();
		if (!context_result) return context_result.error().forward("Initialize context failed");
		auto context = std::move(*context_result);
		auto context_destroyer = std::make_unique<SDLContextDestroyer>();

		/* Step 2: Create SDL window */

		auto window_ptr_result = create_window(window_property);
		if (!window_ptr_result) return window_ptr_result.error().forward("Create window failed");
		auto window = std::unique_ptr<SDL_Window, WindowDeleter>(*window_ptr_result);

		/* Step 3: Instance */

		auto instance_result = create_windowed_instance(context, property, feature, window_property);
		if (!instance_result) return instance_result.error().forward("Create Vulkan instance failed");
		auto instance = std::move(*instance_result);

		/* Step 4: Surface */

		VkSurfaceKHR surface_raw;
		if (!SDL_Vulkan_CreateSurface(window.get(), *instance, nullptr, &surface_raw))
			return Error("Create surface for SDL window failed", SDL_GetError());

		auto surface = std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, SurfaceDeleter>(
			surface_raw,
			SurfaceDeleter{.instance = instance}
		);

		return WindowedInstance(
			std::move(context_destroyer),
			std::move(context),
			std::move(window),
			std::move(instance),
			std::move(surface)
		);
	}

	WindowedInstance::SDLContextDestroyer::~SDLContextDestroyer()
	{
		SDL_Vulkan_UnloadLibrary();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	void WindowedInstance::WindowDeleter::operator()(SDL_Window* ptr) const noexcept
	{
		SDL_DestroyWindow(ptr);
	}

	void WindowedInstance::SurfaceDeleter::operator()(VkSurfaceKHR ptr) const noexcept
	{
		SDL_Vulkan_DestroySurface(instance, ptr, nullptr);
	}
}
