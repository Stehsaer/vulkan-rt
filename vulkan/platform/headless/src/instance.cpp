#include "vulkan/platform/headless/instance.hpp"
#include "common/container/error.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/platform/common/instance.hpp"

#include <expected>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	std::expected<HeadlessInstance, Error> HeadlessInstance::create(
		const InstanceProperty& instance_property,
		const InstanceFeature& instance_feature
	) noexcept
	{
		auto library_result = load_vulkan_library();
		if (!library_result) return library_result.error();
		auto library = std::move(*library_result);

		auto dyn_func_ptr_result = get_vulkan_dyn_func(*library);
		if (!dyn_func_ptr_result) return dyn_func_ptr_result.error();
		auto context = vk::raii::Context(*dyn_func_ptr_result);

		const auto instance_layers = platform::get_instance_layers(instance_feature);
		const auto instance_extensions = platform::get_instance_extensions(instance_feature);

		auto instance_result =
			platform::create_instance(context, instance_property, instance_layers, instance_extensions);
		if (!instance_result) return instance_result.error().forward("Create vulkan instance failed");
		auto instance = std::move(*instance_result);

		return HeadlessInstance(std::move(library), std::move(context), std::move(instance));
	}
}
