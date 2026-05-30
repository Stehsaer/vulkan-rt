#pragma once

#include <cstdint>
#include <mutex>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/alloc/allocator.hpp"

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
	/// @brief Device features
	///
	/// @details
	/// ### Minimal Features
	///
	/// #### Vulkan 1.0
	/// - Robust buffer access
	/// - Sampler anisotropy
	/// - BC textures
	/// - Pipeline statistics
	/// - Multi draw indirect
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
	///
	/// #### Vulkan 1.3
	/// - Dynamic rendering
	/// - Synchronization version 2
	///
	struct DeviceFeature
	{
		/* Required Features */
		// "Support or fail"

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
	/// @brief Common context for rendering and transferring
	/// @warning It's not a good idea to store these references!
	///
	struct Context
	{
		const vk::raii::PhysicalDevice& phy_device;
		const vk::raii::Device& device;
		const vulkan::Allocator& allocator;

		// Main queue, available for compute, transfer and graphics
		const vk::raii::Queue& queue;

		// Mutex for synchronizing queue submissions, use it only when multi-threading
		std::mutex& submit_mutex;

		// Queue family of the main queue
		uint32_t family;

		// Device Feature
		DeviceFeature feature;
	};
}
