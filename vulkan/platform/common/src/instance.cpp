#include "vulkan/platform/common/instance.hpp"
#include "common/container/error.hpp"
#include "common/util/set.hpp"
#include "vulkan/common/config.hpp"
#include "vulkan/common/context.hpp"

#include <expected>
#include <format>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan::platform
{
	std::set<std::string> get_instance_layers(const InstanceFeature& feature) noexcept
	{
		std::set<std::string> layers;
		if (feature.validation) layers.insert("VK_LAYER_KHRONOS_validation");
		return layers;
	}

	std::set<std::string> get_instance_extensions(const InstanceFeature& feature [[maybe_unused]]) noexcept
	{
		// Empty for now
		return {};
	}

	std::expected<vk::raii::Instance, Error> create_instance(
		const vk::raii::Context& context,
		const InstanceProperty& property,
		const std::set<std::string>& requested_layers,
		const std::set<std::string>& requested_extensions
	) noexcept
	{
		using namespace util::set_operator;

		/* Check layers */

		const auto available_layers_result = context.enumerateInstanceLayerProperties();
		if (!available_layers_result) return Error::from(available_layers_result);

		const auto available_layers = *available_layers_result
			| std::views::transform([](const vk::LayerProperties& layer_properties) {
				  return std::string(layer_properties.layerName.data());
			  })
			| std::ranges::to<std::set<std::string>>();

		const auto unsupported_layers = requested_layers - available_layers;
		if (!unsupported_layers.empty())
			return Error("Unsupported instance layers", std::format("{}", unsupported_layers));

		/* Check extensions */

		const auto available_extensions_result = context.enumerateInstanceExtensionProperties();
		if (!available_extensions_result) return Error::from(available_extensions_result);

		const auto available_extensions = *available_extensions_result
			| std::views::transform([](const vk::ExtensionProperties& properties) {
				  return std::string(properties.extensionName.data());
			  })
			| std::ranges::to<std::set<std::string>>();

		const auto unsupported_extensions = requested_extensions - available_extensions;
		if (!unsupported_extensions.empty())
			return Error("Unsupported instance extensions", std::format("{}", unsupported_extensions));

		/* Create instance */

		const auto vk_appinfo = vk::ApplicationInfo{
			.pApplicationName = property.application_name.c_str(),
			.applicationVersion = property.application_version,
			.pEngineName = property.engine_name.c_str(),
			.engineVersion = property.engine_version,
			.apiVersion = config::API_VERSION
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

		auto instance_result = context.createInstance(instance_create_info);
		if (!instance_result) return Error::from(instance_result);
		return std::move(*instance_result);
	}
}
