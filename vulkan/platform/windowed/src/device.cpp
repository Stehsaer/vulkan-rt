#include "vulkan/platform/windowed/device.hpp"
#include "common/container/debug-trace.hpp"
#include "common/container/debug-value.hpp"
#include "common/container/error.hpp"
#include "common/container/linked-struct.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/common/trace/phy-device.hpp"
#include "vulkan/platform/common/device.hpp"
#include "vulkan/platform/windowed/instance.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <libassert/assert.hpp>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	namespace
	{
		struct WindowedDeviceInfo
		{
			vk::raii::PhysicalDevice phy_device;
			util::LinkedStruct<vk::PhysicalDeviceFeatures2> features;
			std::vector<std::string> extensions;
			uint32_t render_family_index;
			uint32_t present_family_index;
			float rank;

			///
			/// @brief Create a surface device and render+present queue
			///
			/// @return `(Device, Render Queue, Present Queue)` or error
			///
			[[nodiscard]]
			std::expected<
				std::tuple<
					vk::raii::Device,
					std::shared_ptr<const DeviceQueue>,
					std::shared_ptr<const DeviceQueue>
				>,
				Error
			> create_device() const noexcept;
		};

		std::expected<
			std::tuple<
				vk::raii::Device,
				std::shared_ptr<const DeviceQueue>,
				std::shared_ptr<const DeviceQueue>
			>,
			Error
		>
		WindowedDeviceInfo::create_device() const noexcept
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

			auto device_result = phy_device.createDevice(device_create_info);
			if (!device_result) return Error::from(device_result);
			auto device = std::move(*device_result);

			std::map<uint32_t, std::shared_ptr<const DeviceQueue>> queue_map;
			for (const auto family : unique_families)
				queue_map.emplace(family, std::make_shared<DeviceQueue>(device.getQueue(family, 0), family));

			return std::make_tuple(
				std::move(device),
				queue_map.at(render_family_index),
				queue_map.at(present_family_index)
			);
		}

		std::expected<WindowedDeviceInfo, Error> check_device(
			const WindowedInstance& instance,
			const vk::raii::PhysicalDevice& phy_device,
			const DeviceFeature& feature
		) noexcept
		{
			using namespace platform;

			const auto wrap_error = [&phy_device](const Error& err) {
				return err
					.forward("Device doesn't meet requirement", std::nullopt, trace_phy_device(phy_device));
			};

			if (const auto check_result = check_device_constraints(phy_device, feature); !check_result)
				return wrap_error(check_result.error());

			auto extensions_result = find_device_extensions(phy_device, feature, true);
			if (!extensions_result) return wrap_error(extensions_result.error());
			auto extensions = std::move(*extensions_result);

			auto features_result = find_device_features(phy_device, feature);
			if (!features_result) return wrap_error(features_result.error());
			auto features = std::move(*features_result);

			auto render_queue_result = find_render_queue(phy_device);
			if (!render_queue_result) return wrap_error(render_queue_result.error());
			const auto render_queue = *render_queue_result;

			auto present_queue_result = find_present_queue(phy_device, instance->surface, render_queue);
			if (!present_queue_result) wrap_error(present_queue_result.error());
			const auto present_queue = *present_queue_result;

			const auto rank = rank_device(phy_device);

			return WindowedDeviceInfo{
				.phy_device = phy_device,
				.features = std::move(features),
				.extensions = std::move(extensions),
				.render_family_index = render_queue,
				.present_family_index = present_queue,
				.rank = rank
			};
		}
	}

	[[nodiscard]]
	static debug::Trace trace_fail(std::span<const Error> list) noexcept
	{
		using debug::operator""_key;

		return {
			"trace"_key =
				list | std::views::transform([](const Error& info) { return debug::value::Error(info); }),
		};
	}

	std::expected<WindowedDevice, Error> WindowedDevice::create(
		const WindowedInstance& context,
		const DeviceFeature& feature
	) noexcept
	{
		/* Enumerate physical devices */

		auto phy_devices_result = context->instance.enumeratePhysicalDevices();
		if (!phy_devices_result) return Error::from(phy_devices_result);
		const auto phy_devices = std::move(*phy_devices_result);

		/* Check each physical device */

		std::vector<WindowedDeviceInfo> pass_devices;
		std::vector<Error> fail_devices;

		for (const auto& phy_device : phy_devices)
		{
			auto check_result = check_device(context, phy_device, feature);
			if (check_result)
				pass_devices.emplace_back(std::move(check_result.value()));
			else
				fail_devices.emplace_back(std::move(check_result.error()));
		}

		if (pass_devices.empty())
			return Error("No suitable device found", std::nullopt, trace_fail(fail_devices));

		/* Find best device and create */

		auto best_device_iter = std::ranges::max_element(pass_devices, {}, &WindowedDeviceInfo::rank);
		ASSERT(best_device_iter != pass_devices.end());
		auto best_device = std::move(*best_device_iter);

		auto device_result = best_device.create_device();
		if (!device_result) return device_result.error().forward("Create device failed");
		auto [device, render_queue, present_queue] = std::move(*device_result);
		const auto phy_device = best_device.phy_device;

		/* Create allocator */

		auto allocator_result = vulkan::Allocator::create(context->instance, phy_device, device);
		if (!allocator_result) return allocator_result.error().forward("Create VMA allocator failed");
		auto allocator = std::move(*allocator_result);

		return WindowedDevice(
			phy_device,
			std::move(device),
			std::move(allocator),
			std::move(render_queue),
			std::move(present_queue),
			feature
		);
	}
}
