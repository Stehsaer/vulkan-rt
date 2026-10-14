#pragma once

#include "common/container/error.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/platform/windowed/instance.hpp"

#include <expected>
#include <memory>
#include <mutex>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Windowed device, designed to render on a window
	/// @note Use @p get_present_queue to get the present queue, and use @p get to get a common context
	///
	class WindowedDevice
	{
	  public:

		///
		/// @brief Create a windowed device context
		///
		/// @param context Windowed instance
		/// @param feature Device feature
		/// @return Create context, or er1ror
		///
		[[nodiscard]]
		static std::expected<WindowedDevice, Error> create(
			const WindowedInstance& context,
			const DeviceFeature& feature
		) noexcept;

		///
		/// @brief Get the present queue
		///
		/// @return Immutable reference to present queue
		///
		[[nodiscard]]
		const DeviceQueue& get_present_queue() const noexcept
		{
			return *present_queue;
		}

		///
		/// @brief Get device context for rendering and transferring
		///
		/// @return Device context for rendering and transferring
		///
		[[nodiscard]]
		Context get() const noexcept
		{
			return {
				.phy_device = *phy_device,
				.device = *device,
				.allocator = *allocator,
				.queue = *render_queue,
				.submit_mutex = *submit_mutex,
				.feature = device_feature,
			};
		}

		[[nodiscard]]
		const vk::raii::Device* operator->() const noexcept
		{
			return device.get();
		}

	  private:

		std::unique_ptr<vk::raii::PhysicalDevice> phy_device;
		std::unique_ptr<vk::raii::Device> device;
		std::unique_ptr<vulkan::Allocator> allocator;
		std::unique_ptr<std::mutex> submit_mutex;

		std::shared_ptr<const DeviceQueue> render_queue;
		std::shared_ptr<const DeviceQueue> present_queue;

		DeviceFeature device_feature;

		explicit WindowedDevice(
			vk::raii::PhysicalDevice phy_device,
			vk::raii::Device device,
			vulkan::Allocator allocator,
			std::shared_ptr<const DeviceQueue> render_queue,
			std::shared_ptr<const DeviceQueue> present_queue,
			DeviceFeature device_feature
		) :
			phy_device(std::make_unique<vk::raii::PhysicalDevice>(std::move(phy_device))),
			device(std::make_unique<vk::raii::Device>(std::move(device))),
			allocator(std::make_unique<vulkan::Allocator>(std::move(allocator))),
			submit_mutex(std::make_unique<std::mutex>()),
			render_queue(std::move(render_queue)),
			present_queue(std::move(present_queue)),
			device_feature(device_feature)
		{}

	  public:

		WindowedDevice(const WindowedDevice&) = delete;
		WindowedDevice(WindowedDevice&&) = default;
		WindowedDevice& operator=(const WindowedDevice&) = delete;
		WindowedDevice& operator=(WindowedDevice&&) = default;
	};
}
