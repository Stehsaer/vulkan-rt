#include "vulkan/context-impl/vulkan.hpp"

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <bit>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/context-impl/window.hpp"

namespace vulkan
{
	static std::vector<std::string> operator-(const std::set<std::string>& a, const std::set<std::string>& b)
	{
		std::vector<std::string> result;
		std::ranges::set_difference(a, b, std::back_inserter(result));
		return result;
	}

#pragma region Features

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

		linked_struct.push<vk::PhysicalDeviceVulkan11Features>({
			.shaderDrawParameters = vk::True,
		});
		linked_struct.push<vk::PhysicalDeviceVulkan13Features>({
			.synchronization2 = vk::True,
			.dynamicRendering = vk::True,
		});
		linked_struct.push<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>({
			.extendedDynamicState = vk::True,
		});

		return linked_struct;
	}

#pragma endregion

	SurfaceWrapper::~SurfaceWrapper() noexcept
	{
		SDL_Vulkan_DestroySurface(instance, surface, nullptr);
	}

	std::expected<vk::raii::Instance, Error> impl::create_instance(
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

		std::set<std::string> requested_layers = features.get_instance_layers();
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

		const auto instance_extensions_expected = impl::get_instance_extensions();
		if (!instance_extensions_expected)
			return instance_extensions_expected.error().forward("Get instance extensions failed");
		requested_extensions.insert_range(*instance_extensions_expected);
		requested_extensions.insert_range(features.get_instance_extensions());
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
		auto instance_expected = context.createInstance(instance_create_info);
		if (!instance_expected) return Error(instance_expected.error(), "Create vulkan instance failed");

		return std::move(*instance_expected);
	}

	std::expected<std::unique_ptr<SurfaceWrapper>, Error> impl::create_surface(
		const vk::raii::Instance& instance,
		SDL_Window* window
	) noexcept
	{
		VkSurfaceKHR surface;
		if (!SDL_Vulkan_CreateSurface(window, *instance, nullptr, &surface))
			return Error("Create surface for SDL window failed");
		return std::make_unique<SurfaceWrapper>(instance, surface);
	}

	static uint32_t score_phy_device(const vk::raii::PhysicalDevice& device)
	{
		uint32_t score = 0;

		const auto properties = device.getProperties();
		if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;
		score += properties.limits.maxImageDimension2D / 512;

		return score;
	}

	std::expected<vk::raii::PhysicalDevice, Error> impl::pick_physical_device(
		const vk::raii::Instance& instance,
		const Features& features
	) noexcept
	{
		/* Predicates */

		const auto extension_available = [&features](const vk::raii::PhysicalDevice& device) {
			const auto extensions = features.get_device_extensions();
			const auto available_extensions = device.enumerateDeviceExtensionProperties()
				| std::views::transform(&vk::ExtensionProperties::extensionName)
				| std::ranges::to<std::set<std::string>>();

			const auto unsupported_extensions = extensions - available_extensions;
			return unsupported_extensions.empty();
		};

		const auto api_version_supported = [](const vk::raii::PhysicalDevice& device) {
			return device.getProperties().apiVersion >= api_version;
		};

		const auto feature_available = [&features](const vk::raii::PhysicalDevice& device) {
			return features.check_device(device);
		};

		/* Detection */

		auto all_devices_expected = instance.enumeratePhysicalDevices();
		if (!all_devices_expected)
			return Error(all_devices_expected.error(), "Enumerate physical devices failed");
		const auto all_devices = std::move(*all_devices_expected);

		const auto available_devices = all_devices
			| std::views::filter(api_version_supported)
			| std::views::filter(extension_available)
			| std::views::filter(feature_available)
			| std::ranges::to<std::vector>();

		if (available_devices.empty()) return Error("No suitable physical device found");

		return std::ranges::max(available_devices, std::less(), score_phy_device);
	}

	static std::optional<uint32_t> find_queue_family_index(
		const vk::raii::PhysicalDevice& device,
		vk::QueueFlagBits required_flags
	)
	{
		const auto queue_families = device.getQueueFamilyProperties();
		for (const auto [idx, queue_family] : std::views::enumerate(queue_families))
			if ((queue_family.queueFlags & required_flags) == required_flags) return idx;
		return std::nullopt;
	}

	std::expected<std::tuple<vk::raii::Device, DeviceQueues>, Error> impl::create_logical_device(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		const Features& features
	) noexcept
	{
		/* Detect Queues */

		const auto graphics_index = find_queue_family_index(phy_device, vk::QueueFlagBits::eGraphics);
		const auto compute_index = find_queue_family_index(phy_device, vk::QueueFlagBits::eCompute);

		if (!graphics_index) return Error("No queue family supports graphics operations");
		if (!compute_index) return Error("No queue family supports compute operations");

		uint32_t present_index;
		if (phy_device.getSurfaceSupportKHR(*graphics_index, surface) == vk::True)
		{
			present_index = *graphics_index;
		}
		else
		{
			const auto queue_families = phy_device.getQueueFamilyProperties();
			auto find = std::ranges::find_if(
				std::views::iota(0u, static_cast<uint32_t>(queue_families.size())),
				[&](uint32_t idx) { return phy_device.getSurfaceSupportKHR(idx, surface) == vk::True; }
			);
			if (*find == static_cast<uint32_t>(queue_families.size()))
				return Error("No queue family supports present operations");

			present_index = *find;
		}

		const std::set<uint32_t> unique_queue_indices = {*graphics_index, *compute_index, present_index};

		const float queue_priority = 1.0f;

		const auto queue_create_infos =
			unique_queue_indices
			| std::views::transform([&queue_priority](uint32_t index) {
				  return vk::DeviceQueueCreateInfo{
					  .queueFamilyIndex = index,
					  .queueCount = 1,
					  .pQueuePriorities = &queue_priority
				  };
			  })
			| std::ranges::to<std::vector>();

		/* Create Device */

		const auto device_features = features.get_device_features_chain();

		const std::vector<std::string> extensions(std::from_range, features.get_device_extensions());
		const std::vector<const char*> extensions_cstr = extensions
			| std::views::transform([](const std::string& ext) { return ext.c_str(); })
			| std::ranges::to<std::vector>();

		const auto device_create_info =
			vk::DeviceCreateInfo{.pNext = device_features.get()}
				.setQueueCreateInfos(queue_create_infos)
				.setPEnabledExtensionNames(extensions_cstr);

		auto device_expected = phy_device.createDevice(device_create_info);
		if (!device_expected) return Error(device_expected.error(), "Create logical device failed");

		/* Retrieve Queues */

		std::map<uint32_t, std::shared_ptr<vk::raii::Queue>> queues_map;
		for (const auto index : unique_queue_indices)
		{
			auto queue_expected = device_expected->getQueue(index, 0);
			if (!queue_expected)
				return Error(queue_expected.error(), std::format("Get queue {} failed", index));

			queues_map[index] = std::make_shared<vk::raii::Queue>(std::move(*queue_expected));
		}

		const DeviceQueues queues{
			.graphics = queues_map.at(*graphics_index),
			.compute = queues_map.at(*compute_index),
			.present = queues_map.at(present_index),

			.graphics_index = *graphics_index,
			.compute_index = *compute_index,
			.present_index = present_index
		};

		return std::make_tuple(std::move(*device_expected), queues);
	}

	static std::optional<vk::SurfaceFormatKHR> select_surface_format(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface
	) noexcept
	{
		const auto available_formats = phy_device.getSurfaceFormatsKHR(surface);
		const auto preferred_formats = std::to_array<vk::SurfaceFormatKHR>({
			{.format = vk::Format::eB8G8R8A8Srgb, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear},
			{.format = vk::Format::eR8G8B8A8Srgb, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear}
		});

		for (const auto& preferred_format : preferred_formats)
		{
			auto found = std::ranges::find_if(
				available_formats,
				[preferred_format](const vk::SurfaceFormatKHR& avail) {
					return std::tie(avail.format, avail.colorSpace)
						== std::tie(preferred_format.format, preferred_format.colorSpace);
				}
			);
			if (found != available_formats.end()) return *found;
		}

		if (!available_formats.empty()) return available_formats.front();

		return std::nullopt;
	}

	static vk::PresentModeKHR select_present_mode(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface
	) noexcept
	{
		const auto available_present_modes = phy_device.getSurfacePresentModesKHR(surface);
		static constexpr auto preferred_present_modes =
			std::to_array({vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eFifoRelaxed});

		for (const auto& preferred_mode : preferred_present_modes)
			if (std::ranges::find(available_present_modes, preferred_mode) != available_present_modes.end())
				return preferred_mode;

		return vk::PresentModeKHR::eFifo;
	}

	std::expected<SwapchainLayout, Error> impl::select_swapchain_layout(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		const DeviceQueues& queues
	) noexcept
	{
		/* Select Sharing Mode */

		vk::SharingMode image_sharing_mode;
		std::vector<uint32_t> image_queue_family_indices;

		if (queues.graphics_index != queues.present_index)
		{
			image_sharing_mode = vk::SharingMode::eConcurrent;
			image_queue_family_indices = {queues.graphics_index, queues.present_index};
		}
		else
		{
			image_sharing_mode = vk::SharingMode::eExclusive;
		}

		/* Find Format */

		auto surface_format_opt = select_surface_format(phy_device, surface);
		if (!surface_format_opt) return Error("No supported surface format found for swapchain");

		/* Select Present Mode */

		const auto present_mode = select_present_mode(phy_device, surface);

		return SwapchainLayout{
			.image_sharing_mode = image_sharing_mode,
			.image_queue_family_indices = image_queue_family_indices,
			.surface_format = *surface_format_opt,
			.present_mode = present_mode
		};
	}

}