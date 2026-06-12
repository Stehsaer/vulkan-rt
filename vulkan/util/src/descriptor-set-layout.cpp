#include "vulkan/util/descriptor-set-layout.hpp"

#include <cstdint>
#include <vulkan/vulkan.hpp>

namespace vulkan
{
	vk::WriteDescriptorSet MonoDescriptorSetLayoutBase::write_descriptor_set(
		vk::DescriptorSet set,
		uint32_t binding,
		vk::DescriptorType type,
		const vk::DescriptorBufferInfo& info
	) noexcept
	{
		return {
			.dstSet = set,
			.dstBinding = binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = type,
			.pBufferInfo = &info
		};
	}

	vk::WriteDescriptorSet MonoDescriptorSetLayoutBase::write_descriptor_set(
		vk::DescriptorSet set,
		uint32_t binding,
		vk::DescriptorType type,
		const vk::DescriptorImageInfo& info
	) noexcept
	{
		return {
			.dstSet = set,
			.dstBinding = binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = type,
			.pImageInfo = &info
		};
	}

	vk::WriteDescriptorSet MonoDescriptorSetLayoutBase::write_descriptor_set(
		vk::DescriptorSet set,
		uint32_t binding,
		vk::DescriptorType type,
		const vk::WriteDescriptorSetAccelerationStructureKHR& info
	) noexcept
	{
		return {
			.pNext = &info,
			.dstSet = set,
			.dstBinding = binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = type
		};
	}
}
