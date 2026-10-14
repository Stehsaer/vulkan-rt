#include "vulkan/platform/headless/device.hpp"
#include "common/container/debug-trace.hpp"
#include "common/container/debug-value.hpp"
#include "common/container/error.hpp"
#include "common/container/linked-struct.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/common/trace/phy-device.hpp"
#include "vulkan/platform/common/device.hpp"
#include "vulkan/platform/headless/instance.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <libassert/assert.hpp>
#include <memory>
#include <optional>
#include <ranges>
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
		struct HeadlessDeviceInfo
		{
			vk::raii::PhysicalDevice phy_device;
			util::LinkedStruct<vk::PhysicalDeviceFeatures2> features;
			std::vector<std::string> extensions;
			uint32_t render_family_index;
			float rank;

			///
			/// @brief Create a headless device and render queue
			///
			/// @return `(Device, Render Queue)` or error
			///
			[[nodiscard]]
			std::expected<
				std::tuple<vk::raii::Device, std::shared_ptr<const DeviceQueue>>,
				Error
			> create_device() const noexcept;
		};

		std::expected<std::tuple<vk::raii::Device, std::shared_ptr<const DeviceQueue>>, Error>
		HeadlessDeviceInfo::create_device() const noexcept
		{
			const float queue_priority = 1.0f;

			const auto queue_create_info = vk::DeviceQueueCreateInfo{
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
					.setQueueCreateInfos(queue_create_info)
					.setPEnabledExtensionNames(extensions_cstr);

			auto device_result = phy_device.createDevice(device_create_info);
			if (!device_result) return Error::from(device_result);
			auto device = std::move(*device_result);

			return std::make_tuple(
				std::move(device),
				std::make_shared<DeviceQueue>(device.getQueue(render_family_index, 0), render_family_index)
			);
		}

		std::expected<HeadlessDeviceInfo, Error> check_device(
			const HeadlessInstance& instance [[maybe_unused]],
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

			auto extensions_result = find_device_extensions(phy_device, feature, false);
			if (!extensions_result) return wrap_error(extensions_result.error());
			auto extensions = std::move(*extensions_result);

			auto features_result = find_device_features(phy_device, feature);
			if (!features_result) return wrap_error(features_result.error());
			auto features = std::move(*features_result);

			auto render_queue_result = find_render_queue(phy_device);
			if (!render_queue_result) return wrap_error(render_queue_result.error());
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

	std::expected<HeadlessDevice, Error> HeadlessDevice::create(
		const HeadlessInstance& instance,
		const DeviceFeature& feature
	) noexcept
	{
		/* Enumerate physical devices */

		auto phy_devices_result = instance->instance.enumeratePhysicalDevices();
		if (!phy_devices_result) return Error::from(phy_devices_result);
		const auto phy_devices = std::move(*phy_devices_result);

		/* Check each physical device */

		std::vector<HeadlessDeviceInfo> pass_devices;
		std::vector<Error> fail_devices;

		for (const auto& phy_device : phy_devices)
		{
			auto check_result = check_device(instance, phy_device, feature);
			if (check_result)
				pass_devices.emplace_back(std::move(check_result.value()));
			else
				fail_devices.emplace_back(std::move(check_result.error()));
		}

		if (pass_devices.empty())
			return Error("No suitable device found", std::nullopt, trace_fail(fail_devices));

		/* Find best device and create */

		auto best_device_iter = std::ranges::max_element(pass_devices, {}, &HeadlessDeviceInfo::rank);
		ASSERT(best_device_iter != pass_devices.end());
		auto best_device = std::move(*best_device_iter);

		auto device_result = best_device.create_device();
		if (!device_result) return device_result.error().forward("Create device failed");
		auto [device, render_queue] = std::move(*device_result);
		const auto phy_device = best_device.phy_device;

		/* Create allocator */

		auto allocator_result = vulkan::Allocator::create(instance->instance, phy_device, device);
		if (!allocator_result) return allocator_result.error().forward("Create VMA allocator failed");
		auto allocator = std::move(*allocator_result);

		return HeadlessDevice(
			phy_device,
			std::move(device),
			std::move(allocator),
			std::move(render_queue),
			feature
		);
	}
}
