#include "vulkan/alloc/allocator.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace vulkan
{
	static VmaAllocationCreateInfo get_allocation_create_info(MemoryUsage usage) noexcept
	{
		VmaAllocationCreateInfo create_info = {};
		create_info.priority = 1.0f;

		switch (usage)
		{
		case MemoryUsage::GpuOnly:
			create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			break;

		case MemoryUsage::CpuToGpu:
			create_info.usage = VMA_MEMORY_USAGE_AUTO;
			create_info.flags =
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;

		case MemoryUsage::GpuToCpu:
			create_info.usage = VMA_MEMORY_USAGE_AUTO;
			create_info.flags =
				VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
		}

		return create_info;
	}

	std::expected<Allocator, Error> Allocator::create(
		const vk::raii::Instance& instance,
		const vk::raii::PhysicalDevice& physical_device,
		const vk::raii::Device& device
	) noexcept
	{
		auto create_info = VmaAllocatorCreateInfo{};

		auto vma_vulkan_funcs = VmaVulkanFunctions{};
		vma_vulkan_funcs.vkGetInstanceProcAddr = instance.getDispatcher()->vkGetInstanceProcAddr;
		vma_vulkan_funcs.vkGetDeviceProcAddr = device.getDispatcher()->vkGetDeviceProcAddr;

		create_info.vulkanApiVersion = vk::ApiVersion14;
		// create_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

		create_info.physicalDevice = *physical_device;
		create_info.device = *device;
		create_info.instance = *instance;
		create_info.pVulkanFunctions = &vma_vulkan_funcs;

		VmaAllocator allocator;
		const auto result = vmaCreateAllocator(&create_info, &allocator);
		if (result != VK_SUCCESS)
			return Error("Create VMA allocator failed", vk::to_string(vk::Result(result)));

		return Allocator(std::make_unique<impl::AllocatorWrapper>(allocator));
	}

	std::expected<Image, Error> Allocator::create_image(
		const vk::ImageCreateInfo& create_info,
		MemoryUsage usage
	) const noexcept
	{
		const VkImageCreateInfo create_info_c = create_info;
		const auto allocation_info = get_allocation_create_info(usage);

		VkImage image;
		VmaAllocation allocation;

		const auto result = vmaCreateImage(
			wrapper->allocator,
			&create_info_c,
			&allocation_info,
			&image,
			&allocation,
			nullptr
		);
		if (result != VK_SUCCESS)
			return Error("Allocate image using VMA failed", vk::to_string(vk::Result(result)));

		return Image(std::make_unique<impl::ImageWrapper>(image, allocation, wrapper->allocator));
	}

	std::expected<Buffer, Error> Allocator::create_buffer(
		const vk::BufferCreateInfo& create_info,
		MemoryUsage usage
	) const noexcept
	{
		const VkBufferCreateInfo create_info_c = create_info;
		const auto allocation_info = get_allocation_create_info(usage);

		VkBuffer buffer;
		VmaAllocation allocation;

		const auto result = vmaCreateBuffer(
			wrapper->allocator,
			&create_info_c,
			&allocation_info,
			&buffer,
			&allocation,
			nullptr
		);
		if (result != VK_SUCCESS)
			return Error("Allocate buffer using VMA failed", vk::to_string(vk::Result(result)));

		return Buffer(std::make_unique<impl::BufferWrapper>(buffer, allocation, wrapper->allocator));
	}

}
