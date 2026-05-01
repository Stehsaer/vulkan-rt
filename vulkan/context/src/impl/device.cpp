#include "impl/device.hpp"
#include "impl/common.hpp"

#include <map>
#include <set>

namespace vulkan::impl
{
#define CHECK_FIELD(available, result, field)                                                                \
	if ((available).field == vk::False) return Error("Required feature '" #field "' is not supported");      \
	(result).field = vk::True;

	[[nodiscard]]
	static std::expected<vk::PhysicalDeviceFeatures, Error> find_vulkan10_features(
		vk::PhysicalDeviceFeatures available,
		const DeviceOption& option [[maybe_unused]]
	) noexcept
	{
		vk::PhysicalDeviceFeatures result{};

		CHECK_FIELD(available, result, robustBufferAccess);
		CHECK_FIELD(available, result, samplerAnisotropy);
		CHECK_FIELD(available, result, textureCompressionBC);
		CHECK_FIELD(available, result, pipelineStatisticsQuery);
		CHECK_FIELD(available, result, multiDrawIndirect);

		return result;
	}

	[[nodiscard]]
	static std::expected<vk::PhysicalDeviceVulkan11Features, Error> find_vulkan11_features(
		vk::PhysicalDeviceVulkan11Features available,
		const DeviceOption& option [[maybe_unused]]
	) noexcept
	{
		vk::PhysicalDeviceVulkan11Features result = {};
		CHECK_FIELD(available, result, shaderDrawParameters);

		return result;
	}

	[[nodiscard]]
	/*NOLINT*/ static std::expected<vk::PhysicalDeviceVulkan12Features, Error> find_vulkan12_features(
		vk::PhysicalDeviceVulkan12Features available,
		const DeviceOption& option
	) noexcept
	{
		vk::PhysicalDeviceVulkan12Features result = {};
		CHECK_FIELD(available, result, shaderFloat16);
		CHECK_FIELD(available, result, scalarBlockLayout);
		CHECK_FIELD(available, result, runtimeDescriptorArray);

		if (option.descriptor_indexing)
		{
			CHECK_FIELD(available, result, descriptorIndexing);
			CHECK_FIELD(available, result, descriptorBindingPartiallyBound);
			CHECK_FIELD(available, result, descriptorBindingVariableDescriptorCount);

			const auto descriptor_indexing_option = *option.descriptor_indexing;

			if (descriptor_indexing_option.sampled_image)
			{
				CHECK_FIELD(available, result, descriptorBindingSampledImageUpdateAfterBind);
				CHECK_FIELD(available, result, shaderSampledImageArrayNonUniformIndexing);
			}

			if (descriptor_indexing_option.storage_image)
			{
				CHECK_FIELD(available, result, descriptorBindingStorageImageUpdateAfterBind);
				CHECK_FIELD(available, result, shaderStorageImageArrayNonUniformIndexing);
			}

			if (descriptor_indexing_option.uniform_buffer)
			{
				CHECK_FIELD(available, result, descriptorBindingUniformBufferUpdateAfterBind);
				CHECK_FIELD(available, result, shaderUniformBufferArrayNonUniformIndexing);
			}

			if (descriptor_indexing_option.storage_buffer)
			{
				CHECK_FIELD(available, result, descriptorBindingStorageBufferUpdateAfterBind);
				CHECK_FIELD(available, result, shaderStorageBufferArrayNonUniformIndexing);
			}
		}

		return result;
	}

	[[nodiscard]]
	static std::expected<vk::PhysicalDeviceVulkan13Features, Error> find_vulkan13_features(
		vk::PhysicalDeviceVulkan13Features available,
		const DeviceOption& option
	) noexcept
	{
		vk::PhysicalDeviceVulkan13Features result = {};
		CHECK_FIELD(available, result, synchronization2);
		if (option.dynamic_rendering) CHECK_FIELD(available, result, dynamicRendering);

		return result;
	}

	[[nodiscard]]
	static std::expected<void, Error> check_device_constraints(
		const vk::raii::PhysicalDevice& phy_device,
		const DeviceOption& option [[maybe_unused]]
	) noexcept
	{
		const auto properties = phy_device.getProperties();

		// Check API version
		if (properties.apiVersion < API_VERSION)
			return Error(
				"Vulkan API version too low",
				std::format(
					"Expecting > {}.{}, got {}.{}",
					VK_VERSION_MAJOR(API_VERSION),
					VK_VERSION_MINOR(API_VERSION),
					VK_VERSION_MAJOR(properties.apiVersion),
					VK_VERSION_MINOR(properties.apiVersion)
				)
			);

		// Check device type
		switch (properties.deviceType)
		{
		case vk::PhysicalDeviceType::eIntegratedGpu:
		case vk::PhysicalDeviceType::eDiscreteGpu:
			break;

		default:
			return Error(
				"Hardware acceleration unavailable",
				std::format("Device type: {}", properties.deviceType)
			);
		}

		// Check device limits
		// (None for now)

		return {};
	}

	[[nodiscard]]
	static std::expected<vulkan::LinkedStruct<vk::PhysicalDeviceFeatures2>, Error> find_device_features(
		const vk::raii::PhysicalDevice& phy_device,
		const DeviceOption& option
	) noexcept
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

		const auto required_features_vulkan10 = find_vulkan10_features(available_features_vulkan10, option);
		const auto required_features_vulkan11 = find_vulkan11_features(available_features_vulkan11, option);
		const auto required_features_vulkan12 = find_vulkan12_features(available_features_vulkan12, option);
		const auto required_features_vulkan13 = find_vulkan13_features(available_features_vulkan13, option);

		if (!required_features_vulkan10.has_value()) return required_features_vulkan10.error();
		if (!required_features_vulkan11.has_value()) return required_features_vulkan11.error();
		if (!required_features_vulkan12.has_value()) return required_features_vulkan12.error();
		if (!required_features_vulkan13.has_value()) return required_features_vulkan13.error();

		return vulkan::LinkedStruct(vk::PhysicalDeviceFeatures2{.features = *required_features_vulkan10})
			.push(*required_features_vulkan11)
			.push(*required_features_vulkan12)
			.push(*required_features_vulkan13);
	}

	constexpr auto mandatory_device_extensions = std::to_array({vk::KHRShaderNonSemanticInfoExtensionName});
	constexpr auto present_mandatory_device_extensions = std::to_array({vk::KHRSwapchainExtensionName});

	[[nodiscard]]
	static std::expected<std::vector<std::string>, Error> find_device_extensions(
		const vk::raii::PhysicalDevice& phy_device,
		const DeviceOption& option [[maybe_unused]],
		bool support_present
	) noexcept
	{
		std::set<std::string> extensions;
		extensions.insert_range(mandatory_device_extensions);
		if (support_present) extensions.insert_range(present_mandatory_device_extensions);

		const auto available_extensions =
			phy_device.enumerateDeviceExtensionProperties()
			| std::views::transform([](const vk::ExtensionProperties& properties) {
				  return std::string(properties.extensionName.data());
			  })
			| std::ranges::to<std::set<std::string>>();

		const auto unsupported_extensions = extensions - available_extensions;
		if (!unsupported_extensions.empty())
			return Error("Missing required device extensions", std::format("{}", unsupported_extensions));

		return extensions | std::ranges::to<std::vector>();
	}

	[[nodiscard]]
	static std::optional<uint32_t> find_queue_family_index(
		const vk::raii::PhysicalDevice& device,
		vk::QueueFlags required_flags
	)
	{
		const auto queue_families = device.getQueueFamilyProperties();
		for (const auto [idx, queue_family] : std::views::enumerate(queue_families))
			if ((queue_family.queueFlags & required_flags) == required_flags) return idx;
		return std::nullopt;
	}

	[[nodiscard]]
	static std::expected<uint32_t, Error> find_render_queue(
		const vk::raii::PhysicalDevice& phy_device
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

		return render_index.value();
	}

	[[nodiscard]]
	static std::expected<uint32_t, Error> find_present_queue(
		const vk::raii::PhysicalDevice& phy_device,
		const VkSurfaceKHR& surface,
		uint32_t render_queue
	) noexcept
	{
		uint32_t present_index;
		if (phy_device.getSurfaceSupportKHR(render_queue, surface) == vk::True)
		{
			present_index = render_queue;
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

		return present_index;
	}

	[[nodiscard]]
	static float rank_device_by_type(const vk::raii::PhysicalDevice& phy_device) noexcept
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
	static float rank_device_by_memory(const vk::raii::PhysicalDevice& phy_device) noexcept
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
	static float rank_device(const vk::raii::PhysicalDevice& phy_device) noexcept
	{
		return rank_device_by_type(phy_device) + rank_device_by_memory(phy_device);
	}

	std::expected<std::pair<vk::raii::Device, DeviceQueue>, Error>
	HeadlessDeviceInfo::create_device() const noexcept
	{
		const float queue_priority = 1.0f;
		const auto render_queue_create_info = vk::DeviceQueueCreateInfo{
			.queueFamilyIndex = render_family_index,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority
		};

		const auto extensions_cstr = extensions
			| std::views::transform([](const auto& str) { return str.c_str(); })
			| std::ranges::to<std::vector>();

		const auto device_create_info =
			vk::DeviceCreateInfo()
				.setPNext(&features.get())
				.setQueueCreateInfos(render_queue_create_info)
				.setPEnabledExtensionNames(extensions_cstr);

		auto device_result =
			phy_device.createDevice(device_create_info).transform_error(Error::from<vk::Result>());
		if (!device_result) return device_result.error().forward("Create device failed");
		auto device = std::move(*device_result);

		auto queue_result =
			device.getQueue(render_family_index, 0).transform_error(Error::from<vk::Result>());
		if (!queue_result) return queue_result.error().forward("Retrieve render queue instance failed");
		auto queue = std::make_shared<vk::raii::Queue>(std::move(*queue_result));

		return std::make_pair(
			std::move(device),
			DeviceQueue{.queue = queue, .family_index = render_family_index}
		);
	}

	std::expected<std::tuple<vk::raii::Device, DeviceQueue, DeviceQueue>, Error>
	SurfaceDeviceInfo::create_device() const noexcept
	{
		const float queue_priority = 1.0f;

		const std::set<uint32_t> unique_families = {render_family_index, present_family_index};
		const auto queue_create_infos =
			unique_families
			| std::views::transform([&queue_priority](uint32_t family_index) {
				  return vk::DeviceQueueCreateInfo{
					  .queueFamilyIndex = family_index,
					  .queueCount = 1,
					  .pQueuePriorities = &queue_priority
				  };
			  })
			| std::ranges::to<std::vector>();

		const auto extensions_cstr = extensions
			| std::views::transform([](const auto& str) { return str.c_str(); })
			| std::ranges::to<std::vector>();

		const auto device_create_info =
			vk::DeviceCreateInfo()
				.setPNext(&features.get())
				.setQueueCreateInfos(queue_create_infos)
				.setPEnabledExtensionNames(extensions_cstr);

		auto device_result =
			phy_device.createDevice(device_create_info).transform_error(Error::from<vk::Result>());
		if (!device_result) return device_result.error().forward("Create device failed");
		auto device = std::move(*device_result);

		std::map<uint32_t, std::shared_ptr<const vk::raii::Queue>> queue_map;
		for (const auto family : unique_families)
		{
			auto queue_result = device.getQueue(family, 0).transform_error(Error::from<vk::Result>());
			if (!queue_result)
				return queue_result.error()
					.forward("Retrieve queue failed", std::format("Queue index {}", family));
			queue_map.emplace(family, std::make_shared<vk::raii::Queue>(std::move(*queue_result)));
		}

		const auto render_queue =
			DeviceQueue{.queue = queue_map.at(render_family_index), .family_index = render_family_index};
		const auto present_queue =
			DeviceQueue{.queue = queue_map.at(present_family_index), .family_index = present_family_index};

		return std::make_tuple(std::move(device), render_queue, present_queue);
	}

	std::variant<HeadlessDeviceInfo, FailInfo> check_headless_device(
		const vk::raii::PhysicalDevice& phy_device,
		const DeviceOption& option
	) noexcept
	{
		if (const auto check_result = check_device_constraints(phy_device, option); !check_result)
			return FailInfo{.phy_device = phy_device, .error = check_result.error()};

		auto extensions_result = find_device_extensions(phy_device, option, false);
		if (!extensions_result) return FailInfo{.phy_device = phy_device, .error = extensions_result.error()};
		auto extensions = std::move(*extensions_result);

		auto features_result = find_device_features(phy_device, option);
		if (!features_result) return FailInfo{.phy_device = phy_device, .error = extensions_result.error()};
		auto features = std::move(*features_result);

		auto render_queue_result = find_render_queue(phy_device);
		if (!render_queue_result)
			return FailInfo{.phy_device = phy_device, .error = extensions_result.error()};
		const auto render_queue = *render_queue_result;

		const auto rank = rank_device(phy_device);

		return HeadlessDeviceInfo{
			.phy_device = phy_device,
			.features = std::move(features),
			.extensions = std::move(extensions),
			.render_family_index = render_queue,
			.rank = rank
		};
	}

	std::variant<SurfaceDeviceInfo, FailInfo> check_surface_device(
		const vk::raii::PhysicalDevice& phy_device,
		const SurfaceInstanceContext& instance,
		const DeviceOption& option
	) noexcept
	{
		if (const auto check_result = check_device_constraints(phy_device, option); !check_result)
			return FailInfo{.phy_device = phy_device, .error = check_result.error()};

		auto extensions_result = find_device_extensions(phy_device, option, true);
		if (!extensions_result) return FailInfo{.phy_device = phy_device, .error = extensions_result.error()};
		auto extensions = std::move(*extensions_result);

		auto features_result = find_device_features(phy_device, option);
		if (!features_result) return FailInfo{.phy_device = phy_device, .error = extensions_result.error()};
		auto features = std::move(*features_result);

		auto render_queue_result = find_render_queue(phy_device);
		if (!render_queue_result)
			return FailInfo{.phy_device = phy_device, .error = extensions_result.error()};
		const auto render_queue = *render_queue_result;

		auto present_queue_result = find_present_queue(phy_device, instance->surface, render_queue);
		if (!present_queue_result)
			return FailInfo{.phy_device = phy_device, .error = present_queue_result.error()};
		const auto present_queue = *present_queue_result;

		const auto rank = rank_device(phy_device);

		return SurfaceDeviceInfo{
			.phy_device = phy_device,
			.features = std::move(features),
			.extensions = std::move(extensions),
			.render_family_index = render_queue,
			.present_family_index = present_queue,
			.rank = rank
		};
	}
}
