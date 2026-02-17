#include "vulkan/context/device.hpp"
#include "impl/common.hpp"
#include "vulkan/context/instance.hpp"
#include "vulkan/util/linked-struct.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <ranges>
#include <set>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	namespace
	{
#define CHECK_FIELD(value, field)                                                                            \
	if ((value).field == vk::False) return Error("Required feature '" #field "' is not supported");

		[[nodiscard]]
		std::expected<vk::PhysicalDeviceFeatures, Error> test_vulkan10_features(
			vk::PhysicalDeviceFeatures available,
			const DeviceContext::Config& config [[maybe_unused]]
		) noexcept
		{
			CHECK_FIELD(available, robustBufferAccess);
			CHECK_FIELD(available, samplerAnisotropy);
			CHECK_FIELD(available, textureCompressionBC);
			CHECK_FIELD(available, pipelineStatisticsQuery);

			return vk::PhysicalDeviceFeatures{
				.robustBufferAccess = vk::True,
				.samplerAnisotropy = vk::True,
				.textureCompressionBC = vk::True,
				.pipelineStatisticsQuery = vk::True,
			};
		}

		[[nodiscard]]
		std::expected<vk::PhysicalDeviceVulkan11Features, Error> test_vulkan11_features(
			vk::PhysicalDeviceVulkan11Features available,
			const DeviceContext::Config& config [[maybe_unused]]
		) noexcept
		{
			CHECK_FIELD(available, shaderDrawParameters);

			return vk::PhysicalDeviceVulkan11Features{
				.shaderDrawParameters = vk::True,
			};
		}

		[[nodiscard]]
		std::expected<vk::PhysicalDeviceVulkan12Features, Error> test_vulkan12_features(
			vk::PhysicalDeviceVulkan12Features available,
			const DeviceContext::Config& config [[maybe_unused]]
		) noexcept
		{
			CHECK_FIELD(available, shaderFloat16);

			return vk::PhysicalDeviceVulkan12Features{
				.shaderFloat16 = vk::True,
			};
		}

		[[nodiscard]]
		std::expected<vk::PhysicalDeviceVulkan13Features, Error> test_vulkan13_features(
			vk::PhysicalDeviceVulkan13Features available,
			const DeviceContext::Config& config [[maybe_unused]]
		) noexcept
		{
			CHECK_FIELD(available, synchronization2);
			CHECK_FIELD(available, dynamicRendering);

			return vk::PhysicalDeviceVulkan13Features{
				.synchronization2 = vk::True,
				.dynamicRendering = vk::True,
			};
		}

		[[nodiscard]]
		std::expected<vulkan::LinkedStruct<vk::PhysicalDeviceFeatures2>, Error> test_device_features(
			const vk::raii::PhysicalDevice& phy_device,
			const DeviceContext::Config& config
		)
		{
			const auto available_features2 = phy_device.getFeatures2<
				vk::PhysicalDeviceFeatures2,
				vk::PhysicalDeviceVulkan11Features,
				vk::PhysicalDeviceVulkan12Features,
				vk::PhysicalDeviceVulkan13Features
			>();

			const auto available_features_vulkan10 =
				available_features2.get<vk::PhysicalDeviceFeatures2>().features;
			const auto available_features_vulkan11 =
				available_features2.get<vk::PhysicalDeviceVulkan11Features>();
			const auto available_features_vulkan12 =
				available_features2.get<vk::PhysicalDeviceVulkan12Features>();
			const auto available_features_vulkan13 =
				available_features2.get<vk::PhysicalDeviceVulkan13Features>();

			const auto required_features_vulkan10 =
				test_vulkan10_features(available_features_vulkan10, config);
			const auto required_features_vulkan11 =
				test_vulkan11_features(available_features_vulkan11, config);
			const auto required_features_vulkan12 =
				test_vulkan12_features(available_features_vulkan12, config);
			const auto required_features_vulkan13 =
				test_vulkan13_features(available_features_vulkan13, config);

			if (!required_features_vulkan10.has_value()) return required_features_vulkan10.error();
			if (!required_features_vulkan11.has_value()) return required_features_vulkan11.error();
			if (!required_features_vulkan12.has_value()) return required_features_vulkan12.error();
			if (!required_features_vulkan13.has_value()) return required_features_vulkan13.error();

			auto linked_struct =
				vulkan::LinkedStruct(vk::PhysicalDeviceFeatures2{.features = *required_features_vulkan10});

			linked_struct.push(*required_features_vulkan11)
				.push(*required_features_vulkan12)
				.push(*required_features_vulkan13);

			return linked_struct;
		}

		constexpr auto mandatory_device_extensions = std::to_array(
			{vk::KHRSwapchainExtensionName,
			 vk::KHRSynchronization2ExtensionName,
			 vk::KHRShaderNonSemanticInfoExtensionName}
		);

		[[nodiscard]]
		std::set<std::string> get_required_extensions(
			const DeviceContext::Config& config [[maybe_unused]]
		) noexcept
		{
			std::set<std::string> extensions;
			extensions.insert_range(mandatory_device_extensions);

			return extensions;
		}

		[[nodiscard]]
		std::expected<std::vector<std::string>, Error> test_device_extensions(
			const vk::raii::PhysicalDevice& phy_device,
			const DeviceContext::Config& config
		) noexcept
		{
			const auto extensions = get_required_extensions(config);
			const auto available_extensions = phy_device.enumerateDeviceExtensionProperties()
				| std::views::transform(&vk::ExtensionProperties::extensionName)
				| std::ranges::to<std::set<std::string>>();

			const auto unsupported_extensions = extensions - available_extensions;
			if (!unsupported_extensions.empty())
				return Error("Missing required device extensions", std::format("{}", unsupported_extensions));

			return extensions | std::ranges::to<std::vector>();
		}

		[[nodiscard]]
		std::expected<void, Error> test_device_limits(
			const vk::raii::PhysicalDevice& phy_device [[maybe_unused]],
			const DeviceContext::Config& config [[maybe_unused]]
		) noexcept
		{
			// TBD
			return {};
		}

		[[nodiscard]]
		std::expected<void, Error> test_device_type(
			const vk::raii::PhysicalDevice& phy_device,
			const DeviceContext::Config& config [[maybe_unused]]
		) noexcept
		{
			const auto properties = phy_device.getProperties();
			if (properties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu
				&& properties.deviceType != vk::PhysicalDeviceType::eIntegratedGpu)
				return Error(
					"Hardware acceleration unavailable",
					std::format("Device type: {}", properties.deviceType)
				);

			return {};
		}

		[[nodiscard]]
		std::optional<uint32_t> find_queue_family_index(
			const vk::raii::PhysicalDevice& device,
			vk::QueueFlags required_flags
		)
		{
			const auto queue_families = device.getQueueFamilyProperties();
			for (const auto [idx, queue_family] : std::views::enumerate(queue_families))
				if ((queue_family.queueFlags & required_flags) == required_flags) return idx;
			return std::nullopt;
		}

		// Returns (render, present)
		[[nodiscard]]
		std::expected<std::tuple<uint32_t, uint32_t>, Error> find_device_queue_families(
			const vk::raii::PhysicalDevice& phy_device,
			const VkSurfaceKHR& surface,
			const DeviceContext::Config& config [[maybe_unused]]
		) noexcept
		{
			const auto render_index = find_queue_family_index(
				phy_device,
				vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer
			);
			if (!render_index)
				return Error(
					"Find render queue family failed",
					"No queue family with graphics & compute & transfer support was found. This is violating "
					"the vulkan spec."
				);

			uint32_t present_index;
			if (phy_device.getSurfaceSupportKHR(*render_index, surface) == vk::True)
			{
				present_index = *render_index;
			}
			else
			{
				const auto queue_families = phy_device.getQueueFamilyProperties();
				auto find = std::ranges::find_if(
					std::views::iota(0u, static_cast<uint32_t>(queue_families.size())),
					[&](uint32_t idx) { return phy_device.getSurfaceSupportKHR(idx, surface) == vk::True; }
				);
				if (*find == static_cast<uint32_t>(queue_families.size()))
					return Error(
						"Find present queue family failed",
						"No queue family with present support was found"
					);

				present_index = *find;
			}

			return std::make_tuple(*render_index, present_index);
		}

		struct DeviceCreateResult
		{
			vk::raii::PhysicalDevice phy_device;
			vk::raii::Device device;
			DeviceContext::Queue render_queue;
			DeviceContext::Queue present_queue;
		};

		struct DeviceCreateInfo
		{
			vk::raii::PhysicalDevice phy_device;
			vulkan::LinkedStruct<vk::PhysicalDeviceFeatures2> features_chain;
			std::vector<std::string> extensions;
			uint32_t render_family_index;
			uint32_t present_family_index;

			[[nodiscard]]
			std::expected<DeviceCreateResult, Error> create_logical_device() const noexcept;
		};

		std::expected<DeviceCreateResult, Error> DeviceCreateInfo::create_logical_device() const noexcept
		{
			const std::set<uint32_t> unique_queue_indices = {render_family_index, present_family_index};

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

			const std::vector<const char*> extensions_cstr = extensions
				| std::views::transform([](const std::string& ext) { return ext.c_str(); })
				| std::ranges::to<std::vector>();

			const auto device_create_info =
				vk::DeviceCreateInfo{.pNext = &features_chain.get()}
					.setQueueCreateInfos(queue_create_infos)
					.setPEnabledExtensionNames(extensions_cstr);

			auto device_result =
				phy_device.createDevice(device_create_info).transform_error(Error::from<vk::Result>());
			if (!device_result) return device_result.error();
			auto device = std::move(*device_result);

			std::map<uint32_t, std::shared_ptr<vk::raii::Queue>> queues_map;
			for (const auto index : unique_queue_indices)
			{
				auto queue_result = device.getQueue(index, 0).transform_error(Error::from<vk::Result>());
				if (!queue_result)
					return queue_result.error().forward(std::format("Get queue at index {} failed", index));

				queues_map[index] = std::make_shared<vk::raii::Queue>(std::move(*queue_result));
			}

			return DeviceCreateResult{
				.phy_device = phy_device,
				.device = std::move(device),
				.render_queue =
					{.queue = queues_map.at(render_family_index),  .family_index = render_family_index },
				.present_queue =
					{.queue = queues_map.at(present_family_index), .family_index = present_family_index},
			};
		}

		[[nodiscard]]
		std::expected<DeviceCreateInfo, Error> test_device_suitability(
			const vk::raii::PhysicalDevice& phy_device,
			const InstanceContext& context,
			const DeviceContext::Config& config
		) noexcept
		{
			auto features_result = test_device_features(phy_device, config);
			if (!features_result)
				return features_result.error().forward("Device does not support required features");

			auto extensions_result = test_device_extensions(phy_device, config);
			if (!extensions_result)
				return extensions_result.error().forward("Device does not support required extensions");

			auto limits_result = test_device_limits(phy_device, config);
			if (!limits_result) return limits_result.error().forward("Device does not meet required limits");

			auto type_result = test_device_type(phy_device, config);
			if (!type_result) return type_result.error().forward("Device is not of a suitable type");

			auto queue_families_result = find_device_queue_families(phy_device, context->surface, config);
			if (!queue_families_result)
				return queue_families_result.error().forward("Device does not have required queue families");
			const auto [render_family_index, present_family_index] = *queue_families_result;

			return DeviceCreateInfo{
				.phy_device = phy_device,
				.features_chain = std::move(*features_result),
				.extensions = std::move(*extensions_result),
				.render_family_index = render_family_index,
				.present_family_index = present_family_index
			};
		}

		[[nodiscard]]
		float rank_device_by_type(const vk::raii::PhysicalDevice& phy_device) noexcept
		{
			const auto properties = phy_device.getProperties();
			switch (properties.deviceType)
			{
			case vk::PhysicalDeviceType::eDiscreteGpu:
				return 2'000'000.0f;
			case vk::PhysicalDeviceType::eIntegratedGpu:
				return 1'000'000.0f;
			default:
				return 0.0f;
			}
		}

		[[nodiscard]]
		float rank_device_by_memory(const vk::raii::PhysicalDevice& phy_device) noexcept
		{
			const auto memory_properties = phy_device.getMemoryProperties();

			auto device_local_heaps =
				memory_properties.memoryHeaps | std::views::filter([](const vk::MemoryHeap& heap) -> bool {
					return (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
						== vk::MemoryHeapFlagBits::eDeviceLocal;
				});

			const auto total_heap_memory_bytes = std::ranges::fold_left(
				device_local_heaps | std::views::transform(&vk::MemoryHeap::size),
				0,
				std::plus()
			);

			return static_cast<float>(total_heap_memory_bytes) / (1024.0f * 1024.0f);
		}

		[[nodiscard]]
		float rank_device(const vk::raii::PhysicalDevice& phy_device) noexcept
		{
			return rank_device_by_type(phy_device) + rank_device_by_memory(phy_device);
		}
	}

	std::expected<DeviceContext, Error> DeviceContext::create(
		const InstanceContext& context,
		const DeviceContext::Config& config
	) noexcept
	{
		/* Step 1: List physical devices */

		auto phy_devices_result =
			context->instance.enumeratePhysicalDevices().transform_error(Error::from<vk::Result>());
		if (!phy_devices_result)
			return phy_devices_result.error().forward("Enumerate physical devices failed");
		const auto phy_devices = std::move(*phy_devices_result);

		/* Step 2: Check suitability */

		auto phy_device_test_results =
			phy_devices
			| std::views::transform([&config, &context](const vk::raii::PhysicalDevice& device) {
				  return std::make_pair(device, test_device_suitability(device, context, config));
			  })
			| std::ranges::to<std::vector>();

		if (std::ranges::none_of(phy_device_test_results, [](const auto& test_result) {
				return test_result.second.has_value();
			}))
		{
			return Error(
				"No suitable physical device found",
				phy_device_test_results
					| std::views::filter([](const auto& test_result) {
						  return !test_result.second.has_value();
					  })
					| std::views::transform([](const auto& test_result) {
						  const auto& [device, result] = test_result;
						  const auto properties = device.getProperties();
						  return std::format(
							  "Device: {}, Type: {}, Suitability check error: {}",
							  properties.deviceName,
							  properties.deviceType,
							  result.error().message
						  );
					  })
					| std::views::join
					| std::ranges::to<std::string>()
			);
		}

		const auto suitable_devices = phy_device_test_results
			| std::views::filter([](const auto& test_result) { return test_result.second.has_value(); })
			| std::views::transform([](auto& test_result) { return std::move(test_result.second.value()); })
			| std::ranges::to<std::vector>();

		/* Step 3: Rank & sort device */

		const auto& best_match_device =
			*std::ranges::max_element(suitable_devices, std::less(), [](const auto& result) {
				return rank_device(result.phy_device);
			});

		/* Step 4: Create logical device */

		auto device_result = best_match_device.create_logical_device();
		if (!device_result) return device_result.error().forward("Create logical device failed");
		auto device = std::move(*device_result);

		/* Step 5: Allocator */

		auto allocator_result =
			vulkan::alloc::Allocator::create(context->instance, device.phy_device, device.device);
		if (!allocator_result) return allocator_result.error().forward("Create allocator failed");
		auto allocator = std::move(*allocator_result);

		return DeviceContext{
			.phy_device = std::move(device.phy_device),
			.device = std::move(device.device),
			.allocator = std::move(allocator),
			.render_queue = std::move(device.render_queue),
			.present_queue = std::move(device.present_queue)
		};
	}
}