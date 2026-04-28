#pragma once

#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vulkan
{
	///
	/// @brief Calculate required pool sizes based on descriptor set layout bindings
	///
	/// @param bindings Descriptor set layout bindings
	/// @param set_count Number of descriptor sets to be allocated from the pool
	/// @return Vector of `vk::DescriptorPoolSize` indicating the required pool sizes for each descriptor type
	///
	[[nodiscard]]
	std::vector<vk::DescriptorPoolSize> calc_pool_sizes(
		std::span<const vk::DescriptorSetLayoutBinding> bindings,
		uint32_t set_count
	) noexcept;
}
