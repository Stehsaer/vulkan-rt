#pragma once

#include "common/container/error.hpp"
#include "vulkan/common/alloc/allocator.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/platform/headless/instance.hpp"

#include <expected>
#include <memory>
#include <mutex>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Headless device for off-screen rendering
	///
	class HeadlessDevice
	{
	  public:

		///
		/// @brief Create a headless device
		///
		/// @param instance Headless instance
		/// @param feature Device feature options
		/// @return Headless device or error
		///
		[[nodiscard]]
		static std::expected<HeadlessDevice, Error> create(
			const HeadlessInstance& instance,
			const DeviceFeature& feature
		) noexcept;

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

		DeviceFeature device_feature;

		explicit HeadlessDevice(
			vk::raii::PhysicalDevice phy_device,
			vk::raii::Device device,
			vulkan::Allocator allocator,
			std::shared_ptr<const DeviceQueue> render_queue,
			DeviceFeature device_feature
		) :
			phy_device(std::make_unique<vk::raii::PhysicalDevice>(std::move(phy_device))),
			device(std::make_unique<vk::raii::Device>(std::move(device))),
			allocator(std::make_unique<vulkan::Allocator>(std::move(allocator))),
			submit_mutex(std::make_unique<std::mutex>()),
			render_queue(std::move(render_queue)),
			device_feature(device_feature)
		{}

	  public:

		HeadlessDevice(const HeadlessDevice&) = delete;
		HeadlessDevice(HeadlessDevice&&) = default;
		HeadlessDevice& operator=(const HeadlessDevice&) = delete;
		HeadlessDevice& operator=(HeadlessDevice&&) = default;
	};
}
