#include "vulkan/context/device.hpp"
#include "common/json.hpp"
#include "common/util/error.hpp"
#include "impl/device.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/context/instance.hpp"

#include <algorithm>
#include <expected>
#include <libassert/assert.hpp>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	[[nodiscard]]
	static Json fail_info_list_to_json(std::span<const impl::FailInfo> list) noexcept
	{
		auto json = Json::array({});
		for (const auto& fail_info : list) json.push_back(fail_info.to_json());
		return json;
	}

	std::expected<HeadlessDeviceContext, Error> HeadlessDeviceContext::create(
		const HeadlessInstanceContext& context,
		const DeviceOption& option
	) noexcept
	{
		/* Enumerate physical devices */

		auto phy_devices_result = context->instance.enumeratePhysicalDevices();
		if (!phy_devices_result) return Error::from(phy_devices_result);
		const auto phy_devices = std::move(*phy_devices_result);

		/* Check each physical device */

		std::vector<impl::HeadlessDeviceInfo> pass_devices;
		std::vector<impl::FailInfo> fail_devices;

		for (const auto& phy_device : phy_devices)
		{
			auto check_result = impl::check_headless_device(phy_device, option);
			if (check_result)
				pass_devices.emplace_back(std::move(check_result.value()));
			else
				fail_devices.emplace_back(std::move(check_result.error()));
		}

		if (pass_devices.empty())
			return Error("No suitable device found", std::nullopt, fail_info_list_to_json(fail_devices));

		/* Find best device and create */

		auto best_device_iter = std::ranges::max_element(pass_devices, {}, &impl::HeadlessDeviceInfo::rank);
		ASSERT(best_device_iter != pass_devices.end());
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

		auto phy_devices_result = context->instance.enumeratePhysicalDevices();
		if (!phy_devices_result) return Error::from(phy_devices_result);
		const auto phy_devices = std::move(*phy_devices_result);

		/* Check each physical device */

		std::vector<impl::SurfaceDeviceInfo> pass_devices;
		std::vector<impl::FailInfo> fail_devices;

		for (const auto& phy_device : phy_devices)
		{
			auto check_result = impl::check_surface_device(phy_device, context, option);
			if (check_result)
				pass_devices.emplace_back(std::move(check_result.value()));
			else
				fail_devices.emplace_back(std::move(check_result.error()));
		}

		if (pass_devices.empty())
			return Error("No suitable device found", std::nullopt, fail_info_list_to_json(fail_devices));

		/* Find best device and create */

		auto best_device_iter = std::ranges::max_element(pass_devices, {}, &impl::SurfaceDeviceInfo::rank);
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

		return SurfaceDeviceContext(
			phy_device,
			std::move(device),
			std::move(allocator),
			std::move(render_queue),
			std::move(present_queue)
		);
	}
}
