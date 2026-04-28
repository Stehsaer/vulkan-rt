#include "vulkan/context/device.hpp"
#include "impl/device.hpp"
#include "vulkan/context/instance.hpp"

#include <algorithm>
#include <ranges>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	std::expected<HeadlessDeviceContext, Error> HeadlessDeviceContext::create(
		const HeadlessInstanceContext& context,
		const DeviceOption& option
	) noexcept
	{
		/* Enumerate physical devices */

		auto phy_devices_result =
			context->instance.enumeratePhysicalDevices().transform_error(Error::from<vk::Result>());
		if (!phy_devices_result)
			return phy_devices_result.error().forward("Enumerate physical devices failed");
		const auto phy_devices = std::move(*phy_devices_result);

		/* Check each physical device */

		std::vector<impl::HeadlessDeviceInfo> pass_devices;
		std::vector<impl::FailInfo> fail_devices;

		for (const auto& phy_device : phy_devices)
		{
			std::visit(
				[&pass_devices, &fail_devices]<typename T>(T result) {
					if constexpr (std::same_as<T, impl::HeadlessDeviceInfo>)
						pass_devices.emplace_back(std::move(result));
					else
						fail_devices.emplace_back(std::move(result));
				},
				impl::check_headless_device(phy_device, option)
			);
		}

		if (pass_devices.empty())
		{
			const auto error_msgs =
				fail_devices | std::views::transform([](const impl::FailInfo& fail_info) {
					const auto device_properties = fail_info.phy_device.getProperties();
					return std::make_pair(
						std::format("{:s}", device_properties.deviceName),
						std::format("{0:msg} - {0:detail}", fail_info.error.root())
					);
				});

			return Error("No suitable device found", std::format("Diagnostics: {}", error_msgs));
		}

		/* Find best device and create */

		auto best_device_iter = std::ranges::max_element(pass_devices, {}, &impl::HeadlessDeviceInfo::rank);
		assert(best_device_iter != pass_devices.end());
		auto best_device = std::move(*best_device_iter);

		auto device_result = best_device.create_device();
		if (!device_result) return device_result.error().forward("Create device failed");
		auto [device, render_queue] = std::move(*device_result);
		const auto phy_device = best_device.phy_device;

		/* Create allocator */

		auto allocator_result = vulkan::Allocator::create(context->instance, phy_device, device);
		if (!allocator_result) return allocator_result.error().forward("Create VMA allocator failed");
		auto allocator = std::move(*allocator_result);

		return HeadlessDeviceContext(
			phy_device,
			std::move(device),
			std::move(allocator),
			std::move(render_queue)
		);
	}

	std::expected<SurfaceDeviceContext, Error> SurfaceDeviceContext::create(
		const SurfaceInstanceContext& context,
		const DeviceOption& option
	) noexcept
	{
		/* Enumerate physical devices */

		auto phy_devices_result =
			context->instance.enumeratePhysicalDevices().transform_error(Error::from<vk::Result>());
		if (!phy_devices_result)
			return phy_devices_result.error().forward("Enumerate physical devices failed");
		const auto phy_devices = std::move(*phy_devices_result);

		/* Check each physical device */

		std::vector<impl::SurfaceDeviceInfo> pass_devices;
		std::vector<impl::FailInfo> fail_devices;

		for (const auto& phy_device : phy_devices)
		{
			std::visit(
				[&pass_devices, &fail_devices]<typename T>(T result) {
					if constexpr (std::same_as<T, impl::SurfaceDeviceInfo>)
						pass_devices.emplace_back(std::move(result));
					else
						fail_devices.emplace_back(std::move(result));
				},
				impl::check_surface_device(phy_device, context, option)
			);
		}

		if (pass_devices.empty())
		{
			const auto error_msgs =
				fail_devices | std::views::transform([](const impl::FailInfo& fail_info) {
					const auto device_properties = fail_info.phy_device.getProperties();
					return std::make_pair(
						std::format("{:s}", device_properties.deviceName),
						std::format("{0:msg} - {0:detail}", fail_info.error.root())
					);
				});

			return Error("No suitable device found", std::format("Diagnostics: {}", error_msgs));
		}

		/* Find best device and create */

		auto best_device_iter = std::ranges::max_element(pass_devices, {}, &impl::SurfaceDeviceInfo::rank);
		assert(best_device_iter != pass_devices.end());
		auto best_device = std::move(*best_device_iter);

		auto device_result = best_device.create_device();
		if (!device_result) return device_result.error().forward("Create device failed");
		auto [device, render_queue, present_queue] = std::move(*device_result);
		const auto phy_device = best_device.phy_device;

		/* Create allocator */

		auto allocator_result = vulkan::Allocator::create(context->instance, phy_device, device);
		if (!allocator_result) return allocator_result.error().forward("Create VMA allocator failed");
		auto allocator = std::move(*allocator_result);

		return SurfaceDeviceContext(
			phy_device,
			std::move(device),
			std::move(allocator),
			std::move(render_queue),
			std::move(present_queue)
		);
	}
}
