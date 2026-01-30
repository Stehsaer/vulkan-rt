#include "vulkan/context/vulkan.hpp"
#include "vulkan/context/window.hpp"

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <bit>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{

	std::set<std::string> Features::get_instance_layers() const noexcept
	{
		std::set<std::string> layers;
		if (validation) layers.insert("VK_LAYER_KHRONOS_validation");
		return layers;
	}

	std::set<std::string> Features::get_instance_extensions() const noexcept
	{
		return {};  // Placeholder, no current features uses it
	}

	vk::PhysicalDeviceFeatures Features::required_vulkan10_features() noexcept
	{
		return {
			.robustBufferAccess = vk::True,
			.samplerAnisotropy = vk::True,
			.textureCompressionBC = vk::True,
			.pipelineStatisticsQuery = vk::True,
		};
	}

	bool Features::check_vulkan10_feature(
		const vk::PhysicalDeviceFeatures& required,
		const vk::PhysicalDeviceFeatures& available
	) noexcept
	{
		using ArrayType = std::array<vk::Bool32, sizeof(vk::PhysicalDeviceFeatures) / sizeof(vk::Bool32)>;

		const auto required_arr = std::bit_cast<ArrayType>(required);
		const auto available_arr = std::bit_cast<ArrayType>(available);

		return std::ranges::equal(required_arr, available_arr, [](vk::Bool32 req, vk::Bool32 avail) {
			return req == vk::False || (req == vk::True && avail == vk::True);
		});
	}

	bool Features::check_device(const vk::raii::PhysicalDevice& phy_device) const noexcept
	{
		const auto available_features2 = phy_device.getFeatures2<
			vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan13Features
		>();

		const auto available_features_vulkan10 =
			available_features2.get<vk::PhysicalDeviceFeatures2>().features;
		const auto available_features_vulkan11 =
			available_features2.get<vk::PhysicalDeviceVulkan11Features>();
		const auto available_features_vulkan13 =
			available_features2.get<vk::PhysicalDeviceVulkan13Features>();

		const auto required_features_vulkan10 = required_vulkan10_features();
		const bool vulkan10_feature_available =
			check_vulkan10_feature(required_features_vulkan10, available_features_vulkan10);

		const bool vulkan11_feature_available = available_features_vulkan11.shaderDrawParameters == vk::True;
		const bool vulkan13_feature_available = available_features_vulkan13.dynamicRendering == vk::True
			&& available_features_vulkan13.synchronization2 == vk::True;

		return vulkan10_feature_available && vulkan11_feature_available && vulkan13_feature_available;
	}

	std::set<std::string> Features::get_device_extensions() const noexcept
	{
		std::set<std::string> extensions;
		extensions.insert_range(mandatory_device_extensions);

		return extensions;
	}

	vulkan::util::LinkedStruct<vk::PhysicalDeviceFeatures2>
	Features::get_device_features_chain() const noexcept
	{
		auto linked_struct = vulkan::util::LinkedStruct<vk::PhysicalDeviceFeatures2>(
			{.features = required_vulkan10_features()}
		);

		linked_struct
			.push<vk::PhysicalDeviceVulkan11Features>({
				.shaderDrawParameters = vk::True,
			})
			.push<vk::PhysicalDeviceVulkan13Features>({
				.synchronization2 = vk::True,
				.dynamicRendering = vk::True,
			})
			.push<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>({
				.extendedDynamicState = vk::True,
			});

		return linked_struct;
	}

	SurfaceWrapper::~SurfaceWrapper() noexcept
	{
		SDL_Vulkan_DestroySurface(instance, surface, nullptr);
	}
}