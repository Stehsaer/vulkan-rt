#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/common/alloc/allocator.hpp"
#include "vulkan/common/config.hpp"

namespace vulkan
{
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
	/// @brief Instance properties
	///
	struct InstanceProperty
	{
		std::string application_name = "Vulkan Application";
		std::string engine_name = "No Engine";
		uint32_t application_version = vk::makeVersion(0, 0, 0);
		uint32_t engine_version = vk::makeVersion(0, 0, 0);
	};

	///
	/// @brief Instance feature
	/// @note Some layers and extensions are not controlled by this struct
	///
	struct InstanceFeature
	{
		bool validation = config::DEFAULT_VALIDATION_ENABLED;
	};

	///
	/// @brief Device features
	///
	/// @details
	/// ### Minimal Features
	///
	/// #### Vulkan 1.0
	/// - Sampler anisotropy
	/// - BC textures
	/// - Pipeline statistics
	/// - Multi draw indirect
	/// - Shader 16-bit integer support
	/// - Extended image gather
	///
	/// #### Vulkan 1.1
	/// - Shader draw parameters (required by slang)
	///
	/// #### Vulkan 1.2
	/// - Half-precision float in shader
	/// - Scalar shader block layout
	/// - Runtime descriptor array
	/// - Descriptor indexing (basic supports)
	/// - Buffer device address
	/// - Shader 8-bit integer support
	///
	/// #### Vulkan 1.3
	/// - Dynamic rendering
	/// - Synchronization version 2
	///
	struct DeviceFeature
	{
		/* Required Features */
		// "Support or fail"

		///
		/// @brief Robust buffer access
		/// @note Enabled by default in debug, disabled by default in release. Beware of extra runtime cost
		///
		bool robust_buffer_access = config::DEFAULT_ROBUST_BUFFER_ACCESS_ENABLED;

		///
		/// @brief Descriptor indexing option
		///
		DescriptorIndexingOption descriptor_indexing = {};

		///
		/// @brief Raytracing feature
		/// @details Implies:
		/// - Acceleration structure (BLAS/TLAS)
		///
		/// and also their dependencies
		///
		bool raytracing = false;

		/* On-demand Features */
		// Runs on fallback policies if not present
	};

	///
	/// @brief Device queue wrapper, containing a queue and its queue family
	///
	struct DeviceQueue : public vk::raii::Queue
	{
		///
		/// @brief Queue family of the queue
		///
		uint32_t family;

		explicit DeviceQueue(vk::raii::Queue queue, uint32_t family) :
			vk::raii::Queue(std::move(queue)),
			family(family)
		{}

		DeviceQueue(const DeviceQueue&) = default;
		DeviceQueue(DeviceQueue&&) = default;
		DeviceQueue& operator=(const DeviceQueue&) = default;
		DeviceQueue& operator=(DeviceQueue&&) = default;
	};

	///
	/// @brief Common context for rendering and transferring
	/// @warning It's not a good idea to store these references!
	///
	struct Context
	{
		const vk::raii::PhysicalDevice& phy_device;
		const vk::raii::Device& device;
		const vulkan::Allocator& allocator;

		// Main queue, available for compute, transfer and graphics
		const DeviceQueue& queue;

		// Mutex for synchronizing queue submissions, use it only when multi-threading
		std::mutex& submit_mutex;

		// Device Feature
		DeviceFeature feature;
	};
}
