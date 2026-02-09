#include "impl/instance.hpp"
#include "impl/common.hpp"

#include <SDL3/SDL_vulkan.h>

namespace vulkan::context
{
	namespace
	{
		std::set<std::string> get_instance_layers(const Features& features) noexcept
		{
			std::set<std::string> layers;
			if (features.validation) layers.insert("VK_LAYER_KHRONOS_validation");
			return layers;
		}

		std::expected<std::set<std::string>, Error> get_instance_extensions(
			const Features& features [[maybe_unused]]
		) noexcept
		{
			unsigned int extension_count = 0;
			const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
			if (extensions == nullptr) return Error("Get instance extensions failed", SDL_GetError());

			std::set<std::string> extension_set;
			extension_set.insert_range(std::span(extensions, extension_count));

			return extension_set;
		}
	}

	std::expected<vk::raii::Instance, Error> create_instance(
		const vk::raii::Context& context,
		const AppInfo& app_info,
		const Features& features
	) noexcept
	{
		/* App Info*/

		const auto vk_appinfo = vk::ApplicationInfo{
			.pApplicationName = app_info.application_name.c_str(),
			.applicationVersion = app_info.application_version,
			.pEngineName = app_info.engine_name.c_str(),
			.engineVersion = app_info.engine_version,
			.apiVersion = api_version
		};

		/* Check Instance Layers */

		std::set<std::string> requested_layers = get_instance_layers(features);
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

		auto instance_extensions_result = get_instance_extensions(features);
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
		if (!instance_result) return instance_result.error().forward("Create Vulkan instance failed");
		auto instance = std::move(*instance_result);

		return instance;
	}
}