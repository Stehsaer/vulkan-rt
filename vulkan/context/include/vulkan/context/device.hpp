#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc.hpp"
#include "vulkan/context/instance.hpp"

#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	struct DeviceQueue
	{
		std::shared_ptr<const vk::raii::Queue> queue;
		uint32_t family_index;
	};

	struct DeviceConfig
	{
		// Empty for now
	};

	///
	/// @brief Abstract device context, hosts the most common ingredients for graphics rendering
	/// @note Use `operator->` to access references to the members
	///
	class DeviceContext
	{
	  private:

		struct VisitProxy
		{
			const vk::raii::PhysicalDevice& phy_device;
			const vk::raii::Device& device;
			const vulkan::alloc::Allocator& allocator;
			const DeviceQueue& render_queue;

			const VisitProxy* operator->() const noexcept { return this; }
		};

	  protected:

		std::unique_ptr<vk::raii::PhysicalDevice> phy_device;
		std::unique_ptr<vk::raii::Device> device;
		std::unique_ptr<vulkan::alloc::Allocator> allocator;

		std::unique_ptr<DeviceQueue> render_queue;

		explicit DeviceContext(
			vk::raii::PhysicalDevice phy_device,
			vk::raii::Device device,
			vulkan::alloc::Allocator allocator,
			DeviceQueue render_queue
		) :
			phy_device(std::make_unique<vk::raii::PhysicalDevice>(std::move(phy_device))),
			device(std::make_unique<vk::raii::Device>(std::move(device))),
			allocator(std::make_unique<vulkan::alloc::Allocator>(std::move(allocator))),
			render_queue(std::make_unique<DeviceQueue>(std::move(render_queue)))
		{}

	  public:

		VisitProxy operator->() const noexcept
		{
			return {
				.phy_device = *phy_device,
				.device = *device,
				.allocator = *allocator,
				.render_queue = *render_queue
			};
		}

		DeviceContext(const DeviceContext&) = delete;
		DeviceContext(DeviceContext&&) = default;
		DeviceContext& operator=(const DeviceContext&) = delete;
		DeviceContext& operator=(DeviceContext&&) = default;
	};

	///
	/// @brief Headless device context, designed to render without a window
	///
	///
	class HeadlessDeviceContext : public DeviceContext
	{
	  public:

		[[nodiscard]]
		static std::expected<HeadlessDeviceContext, Error> create(
			const HeadlessInstanceContext& context,
			const DeviceConfig& config
		) noexcept;

	  protected:

		HeadlessDeviceContext(
			vk::raii::PhysicalDevice phy_device,
			vk::raii::Device device,
			vulkan::alloc::Allocator allocator,
			DeviceQueue render_queue
		) :
			DeviceContext(
				std::move(phy_device),
				std::move(device),
				std::move(allocator),
				std::move(render_queue)
			)
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
	class SurfaceDeviceContext : public DeviceContext
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
			const DeviceConfig& config
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

	  private:

		std::unique_ptr<DeviceQueue> present_queue;

		explicit SurfaceDeviceContext(
			vk::raii::PhysicalDevice phy_device,
			vk::raii::Device device,
			vulkan::alloc::Allocator allocator,
			DeviceQueue render_queue,
			DeviceQueue present_queue
		) :
			DeviceContext(
				std::move(phy_device),
				std::move(device),
				std::move(allocator),
				std::move(render_queue)
			),
			present_queue(std::make_unique<DeviceQueue>(std::move(present_queue)))
		{}

	  public:

		SurfaceDeviceContext(const SurfaceDeviceContext&) = delete;
		SurfaceDeviceContext(SurfaceDeviceContext&&) = default;
		SurfaceDeviceContext& operator=(const SurfaceDeviceContext&) = delete;
		SurfaceDeviceContext& operator=(SurfaceDeviceContext&&) = default;
	};
}
