#include "vulkan/context/instance.hpp"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <string>
#include <vulkan/vulkan_raii.hpp>

#include "impl/common.hpp"

namespace vulkan
{
	static SDL_WindowFlags get_flags(const InstanceContext::Config& config) noexcept
	{
		SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
		if (config.resizable) flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_RESIZABLE);
		if (config.initial_fullscreen) flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_FULLSCREEN);
		return flags;
	}

	static std::set<std::string> get_instance_layers(const InstanceContext::Config& config) noexcept
	{
		std::set<std::string> layers;
		if (config.validation) layers.insert("VK_LAYER_KHRONOS_validation");
		return layers;
	}

	static std::expected<std::set<std::string>, Error> get_instance_extensions(
		const InstanceContext::Config& features [[maybe_unused]]
	) noexcept
	{
		unsigned int extension_count = 0;
		const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
		if (extensions == nullptr) return Error("Get instance extensions failed", SDL_GetError());

		std::set<std::string> extension_set;
		extension_set.insert_range(std::span(extensions, extension_count));

		return extension_set;
	}

	static std::expected<SDL_Window*, Error> create_window(const InstanceContext::Config& config) noexcept
	{
		if (!SDL_Init(SDL_INIT_VIDEO)) return Error("Initialize SDL failed", SDL_GetError());

		const auto window_ptr = SDL_CreateWindow(
			config.title.c_str(),
			config.initial_size.x,
			config.initial_size.y,
			get_flags(config)
		);
		if (window_ptr == nullptr) return Error("Create SDL window failed", SDL_GetError());

		return window_ptr;
	}

	static std::expected<vk::raii::Instance, Error> create_instance(
		const vk::raii::Context& context,
		const InstanceContext::Config& config
	) noexcept
	{
		/* App Info*/

		const auto vk_appinfo = vk::ApplicationInfo{
			.pApplicationName = config.application_name.c_str(),
			.applicationVersion = config.application_version,
			.pEngineName = config.engine_name.c_str(),
			.engineVersion = config.engine_version,
			.apiVersion = api_version
		};

		/* Check Instance Layers */

		std::set<std::string> requested_layers = get_instance_layers(config);
		{
			const auto available_layers = context.enumerateInstanceLayerProperties()
				| std::views::transform(&vk::LayerProperties::layerName)
				| std::ranges::to<std::set<std::string>>();

			const auto unsupported_layers = requested_layers - available_layers;
			if (!unsupported_layers.empty())
				return Error(std::format("Unsupported instance layers: {}", unsupported_layers));
		}

		/* Check Instance Extensions */

		std::set<std::string> requested_extensions;

		auto instance_extensions_result = get_instance_extensions(config);
		if (!instance_extensions_result)
			return instance_extensions_result.error().forward("Get instance extensions failed");
		const auto instance_extensions = std::move(*instance_extensions_result);
		requested_extensions.insert_range(instance_extensions);
		{
			const auto available_extensions = context.enumerateInstanceExtensionProperties()
				| std::views::transform(&vk::ExtensionProperties::extensionName)
				| std::ranges::to<std::set<std::string>>();

			const auto unsupported_extensions = requested_extensions - available_extensions;
			if (!unsupported_extensions.empty())
				return Error(std::format("Unsupported instance extensions: {}", unsupported_extensions));
		}

		/* Create Instance */

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

		auto instance_result =
			context.createInstance(instance_create_info).transform_error(Error::from<vk::Result>());
		if (!instance_result) return instance_result.error();
		auto instance = std::move(*instance_result);

		return instance;
	}

	std::expected<InstanceContext, Error> InstanceContext::create(const Config& config) noexcept
	{
		/* Step 1: Vulkan Context */

		vk::raii::Context context;

		/* Step 2: SDL Initialization */

		auto window_ptr_result = create_window(config);
		if (!window_ptr_result) return window_ptr_result.error().forward("Create window failed");
		auto window = std::make_unique<WindowWrapper>(*window_ptr_result);

		/* Step 3: Instance */

		auto instance_result = create_instance(context, config);
		if (!instance_result) return instance_result.error().forward("Create Vulkan instance failed");
		auto instance = std::move(*instance_result);

		/* Step 4: Surface */

		VkSurfaceKHR surface_raw;
		if (!SDL_Vulkan_CreateSurface(window->get(), *instance, nullptr, &surface_raw))
			return Error("Create surface for SDL window failed", SDL_GetError());
		auto surface = std::make_unique<SurfaceWrapper>(*instance, surface_raw);

		return InstanceContext(
			std::move(context),
			std::move(window),
			std::move(instance),
			std::move(surface)
		);
	}

	InstanceContext::SurfaceWrapper::~SurfaceWrapper() noexcept
	{
		SDL_Vulkan_DestroySurface(instance, surface, nullptr);
	}

	InstanceContext::WindowWrapper::~WindowWrapper() noexcept
	{
		SDL_DestroyWindow(window);
		SDL_Vulkan_UnloadLibrary();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}
