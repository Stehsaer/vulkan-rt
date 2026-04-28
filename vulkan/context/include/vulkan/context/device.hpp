#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/context/instance.hpp"
#include "vulkan/interface/common-context.hpp"

#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	struct DeviceQueue
	{
		std::shared_ptr<const vk::raii::Queue> queue;
		uint32_t family_index;
	};

	///
	/// @brief Option for descriptor indexing features
	///
	///
	struct DescriptorIndexingOption
	{
		bool sampled_image = true;
		bool storage_image = false;
		bool uniform_buffer = false;
		bool storage_buffer = false;
	};

	///
	/// @brief Device creation options
	///
	///
	struct DeviceOption
	{
		bool dynamic_rendering = true;
		std::optional<DescriptorIndexingOption> descriptor_indexing = DescriptorIndexingOption();
	};

	///
	/// @brief Headless device context, designed to render without a window
	///
	///
	class HeadlessDeviceContext
	{
	  public:

		[[nodiscard]]
		static std::expected<HeadlessDeviceContext, Error> create(
			const HeadlessInstanceContext& context,
			const DeviceOption& option
		) noexcept;

		///
		/// @brief Get device context for rendering and transferring
		///
		/// @return Device context for rendering and transferring
		///
		[[nodiscard]]
		DeviceContext get() const noexcept
		{
			return {
				.phy_device = *phy_device,
				.device = *device,
				.allocator = *allocator,
				.queue = *render_queue.queue,
				.submit_mutex = *submit_mutex,
				.family = render_queue.family_index,
			};
		}

	  private:

		std::unique_ptr<vk::raii::PhysicalDevice> phy_device;
		std::unique_ptr<vk::raii::Device> device;
		std::unique_ptr<vulkan::Allocator> allocator;
		std::unique_ptr<std::mutex> submit_mutex;

		DeviceQueue render_queue;

		HeadlessDeviceContext(
			vk::raii::PhysicalDevice phy_device,
			vk::raii::Device device,
			vulkan::Allocator allocator,
			DeviceQueue render_queue
		) :
			phy_device(std::make_unique<vk::raii::PhysicalDevice>(std::move(phy_device))),
			device(std::make_unique<vk::raii::Device>(std::move(device))),
			allocator(std::make_unique<vulkan::Allocator>(std::move(allocator))),
			submit_mutex(std::make_unique<std::mutex>()),
			render_queue(std::move(render_queue))
		{}

	  public:

		HeadlessDeviceContext(const HeadlessDeviceContext&) = delete;
		HeadlessDeviceContext(HeadlessDeviceContext&&) = default;
		HeadlessDeviceContext& operator=(const HeadlessDeviceContext&) = delete;
		HeadlessDeviceContext& operator=(HeadlessDeviceContext&&) = default;
	};

	///
	/// @brief Surface device context, designed to render on a window
	/// @note Use @p get_present_queue to get the present queue, and use `operator->` to get the render queue
	///
	class SurfaceDeviceContext
	{
	  public:

		///
		/// @brief Create a surface device context
		///
		/// @param context Surface instance
		/// @param config Device config
		/// @return Create context, or error
		///
		[[nodiscard]]
		static std::expected<SurfaceDeviceContext, Error> create(
			const SurfaceInstanceContext& context,
			const DeviceOption& option
		) noexcept;

		///
		/// @brief Get the present queue
		///
		/// @return Immutable reference to present queue
		///
		[[nodiscard]]
		const DeviceQueue& get_present_queue() const noexcept
		{
			return present_queue;
		}

		///
		/// @brief Get device context for rendering and transferring
		///
		/// @return Device context for rendering and transferring
		///
		[[nodiscard]]
		DeviceContext get() const noexcept
		{
			return {
				.phy_device = *phy_device,
				.device = *device,
				.allocator = *allocator,
				.queue = *render_queue.queue,
				.submit_mutex = *submit_mutex,
				.family = render_queue.family_index,
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

		DeviceQueue render_queue;
		DeviceQueue present_queue;

		explicit SurfaceDeviceContext(
			vk::raii::PhysicalDevice phy_device,
			vk::raii::Device device,
			vulkan::Allocator allocator,
			DeviceQueue render_queue,
			DeviceQueue present_queue
		) :
			phy_device(std::make_unique<vk::raii::PhysicalDevice>(std::move(phy_device))),
			device(std::make_unique<vk::raii::Device>(std::move(device))),
			allocator(std::make_unique<vulkan::Allocator>(std::move(allocator))),
			submit_mutex(std::make_unique<std::mutex>()),
			render_queue(std::move(render_queue)),
			present_queue(std::move(present_queue))
		{}

	  public:

		SurfaceDeviceContext(const SurfaceDeviceContext&) = delete;
		SurfaceDeviceContext(SurfaceDeviceContext&&) = default;
		SurfaceDeviceContext& operator=(const SurfaceDeviceContext&) = delete;
		SurfaceDeviceContext& operator=(SurfaceDeviceContext&&) = default;
	};
}
