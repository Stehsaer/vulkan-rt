#include "impl/device.hpp"
#include "impl/common.hpp"
#include "vulkan/util/linked-struct.hpp"

#include <map>

namespace vulkan::context
{
	namespace
	{
		constexpr auto required_vulkan10_features = vk::PhysicalDeviceFeatures{
			.robustBufferAccess = vk::True,
			.samplerAnisotropy = vk::True,
			.textureCompressionBC = vk::True,
			.pipelineStatisticsQuery = vk::True,
		};

		constexpr auto mandatory_device_extensions =
			std::to_array({vk::KHRSwapchainExtensionName, vk::KHRSynchronization2ExtensionName});

		std::set<std::string> get_device_extensions(const Features& features [[maybe_unused]]) noexcept
		{
			std::set<std::string> extensions;
			extensions.insert_range(mandatory_device_extensions);

			return extensions;
		}

		bool check_vulkan10_feature(
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

		bool check_device(
			const Features& features [[maybe_unused]],
			const vk::raii::PhysicalDevice& phy_device
		) noexcept
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

			const bool vulkan10_feature_available =
				check_vulkan10_feature(required_vulkan10_features, available_features_vulkan10);

			const bool vulkan11_feature_available =
				available_features_vulkan11.shaderDrawParameters == vk::True;
			const bool vulkan13_feature_available = available_features_vulkan13.dynamicRendering == vk::True
				&& available_features_vulkan13.synchronization2 == vk::True;

			return vulkan10_feature_available && vulkan11_feature_available && vulkan13_feature_available;
		}

		vulkan::util::LinkedStruct<vk::PhysicalDeviceFeatures2> get_device_features_chain(
			const Features& features [[maybe_unused]]
		) noexcept
		{
			auto linked_struct = vulkan::util::LinkedStruct<vk::PhysicalDeviceFeatures2>(
				{.features = required_vulkan10_features}
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

		uint32_t score_phy_device(const vk::raii::PhysicalDevice& device)
		{
			uint32_t score = 0;

			const auto properties = device.getProperties();
			if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;
			score += properties.limits.maxImageDimension2D / 512;

			return score;
		}

		std::optional<uint32_t> find_queue_family_index(
			const vk::raii::PhysicalDevice& device,
			vk::QueueFlagBits required_flags
		)
		{
			const auto queue_families = device.getQueueFamilyProperties();
			for (const auto [idx, queue_family] : std::views::enumerate(queue_families))
				if ((queue_family.queueFlags & required_flags) == required_flags) return idx;
			return std::nullopt;
		}
	}

	std::expected<vk::raii::PhysicalDevice, Error> pick_physical_device(
		const vk::raii::Instance& instance,
		const Features& features
	) noexcept
	{
		/* Predicates */

		const auto extension_available = [&features](const vk::raii::PhysicalDevice& device) {
			const auto extensions = get_device_extensions(features);
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
			return check_device(features, device);
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

	std::expected<std::tuple<vk::raii::Device, DeviceQueues>, Error> create_logical_device(
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

		const auto device_features = get_device_features_chain(features);

		const std::vector<std::string> extensions(std::from_range, get_device_extensions(features));
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
}