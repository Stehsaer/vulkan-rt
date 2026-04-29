#include "impl/instance.hpp"
#include "impl/common.hpp"
#include "vulkan/context/instance.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <string>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan::impl
{
	// Get SDL window flags
	static SDL_WindowFlags get_flags_sdl(const WindowConfig& config) noexcept
	{
		SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
		if (config.resizable) flags |= SDL_WINDOW_RESIZABLE;
		if (config.initial_fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
		return flags;
	}

	// Get instance layers
	static std::set<std::string> get_instance_layers(const InstanceConfig& config) noexcept
	{
		std::set<std::string> layers;
		if (config.validation) layers.insert("VK_LAYER_KHRONOS_validation");
		return layers;
	}

	// Get instance layers from SDL config
	static std::expected<std::set<std::string>, Error> get_instance_layers_sdl(
		const WindowConfig& config [[maybe_unused]]
	) noexcept
	{
		// Empty for now
		return std::set<std::string>();
	}

	// Get instance extensions
	static std::set<std::string> get_instance_extensions(
		const InstanceConfig& config [[maybe_unused]]
	) noexcept
	{
		// Empty for now
		return {};
	}

	// Get instance extensions from sdl config
	static std::expected<std::set<std::string>, Error> get_instance_extensions_sdl(
		const WindowConfig& config [[maybe_unused]]
	) noexcept
	{
		unsigned int extension_count = 0;
		const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
		if (extensions == nullptr) return Error("Get instance extensions failed", SDL_GetError());

		return std::set<std::string>(std::from_range, std::span(extensions, extension_count));
	}

	// Create a vulkan instance by providing layers and extensions
	static std::expected<vk::raii::Instance, Error> create_instance(
		const vk::raii::Context& context,
		const InstanceConfig& config,
		const std::set<std::string>& requested_layers,
		const std::set<std::string>& requested_extensions
	) noexcept
	{
		/* Check layers */

		const auto available_layers =
			context.enumerateInstanceLayerProperties()
			| std::views::transform([](const vk::LayerProperties& layer_properties) {
				  return std::string(layer_properties.layerName.data());
			  })
			| std::ranges::to<std::set<std::string>>();

		const auto unsupported_layers = requested_layers - available_layers;
		if (!unsupported_layers.empty())
			return Error("Unsupported instance layers", std::format("{}", unsupported_layers));

		/* Check extensions */

		const auto available_extensions =
			context.enumerateInstanceExtensionProperties()
			| std::views::transform([](const vk::ExtensionProperties& properties) {
				  return std::string(properties.extensionName.data());
			  })
			| std::ranges::to<std::set<std::string>>();

		const auto unsupported_extensions = requested_extensions - available_extensions;
		if (!unsupported_extensions.empty())
			return Error("Unsupported instance extensions", std::format("{}", unsupported_extensions));

		/* Create instance */

		const auto vk_appinfo = vk::ApplicationInfo{
			.pApplicationName = config.application_name.c_str(),
			.applicationVersion = config.application_version,
			.pEngineName = config.engine_name.c_str(),
			.engineVersion = config.engine_version,
			.apiVersion = API_VERSION
		};

		const auto requested_layers_vec = requested_layers
			| std::views::transform([](const std::string& layer) { return layer.c_str(); })
			| std::ranges::to<std::vector>();
		const auto requested_extensions_vec = requested_extensions
			| std::views::transform([](const std::string& layer) { return layer.c_str(); })
			| std::ranges::to<std::vector>();

		const auto instance_create_info =
			vk::InstanceCreateInfo{.pApplicationInfo = &vk_appinfo}
				.setPEnabledLayerNames(requested_layers_vec)
				.setPEnabledExtensionNames(requested_extensions_vec);

		return context.createInstance(instance_create_info).transform_error(Error::from<vk::Result>());
	}

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

	std::expected<SDL_Window*, Error> create_window(const WindowConfig& config) noexcept
	{
		const auto window_ptr = SDL_CreateWindow(
			config.title.c_str(),
			config.initial_size.x,
			config.initial_size.y,
			get_flags_sdl(config)
		);
		if (window_ptr == nullptr) return Error("Create SDL window failed", SDL_GetError());

		return window_ptr;
	}

	std::expected<vk::raii::Instance, Error> create_instance_headless(
		const vk::raii::Context& context,
		const InstanceConfig& instance_config
	) noexcept
	{
		const auto instance_layers = get_instance_layers(instance_config);
		const auto instance_extensions = get_instance_extensions(instance_config);

		return create_instance(context, instance_config, instance_layers, instance_extensions);
	}

	std::expected<vk::raii::Instance, Error> create_instance_surface(
		const vk::raii::Context& context,
		const InstanceConfig& instance_config,
		const WindowConfig& window_config
	) noexcept
	{
		const auto instance_layers = get_instance_layers(instance_config);
		const auto instance_extensions = get_instance_extensions(instance_config);

		const auto instance_layers_sdl_result = get_instance_layers_sdl(window_config);
		const auto instance_extensions_sdl_result = get_instance_extensions_sdl(window_config);

		if (!instance_layers_sdl_result)
			return instance_layers_sdl_result.error().forward("Get instance layers from SDL failed");
		if (!instance_extensions_sdl_result)
			return instance_extensions_sdl_result.error().forward("Get instance extensions from SDL failed");

		const auto layers = instance_layers + *instance_layers_sdl_result;
		const auto extensions = instance_extensions + *instance_extensions_sdl_result;

		return create_instance(context, instance_config, layers, extensions);
	}
}
