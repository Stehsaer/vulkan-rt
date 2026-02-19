#include "vulkan/alloc.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace vulkan::alloc
{
	std::expected<Allocator, Error> Allocator::create(
		const vk::Instance& instance,
		const vk::PhysicalDevice& physical_device,
		const vk::Device& device
	) noexcept
	{
		auto create_info = VmaAllocatorCreateInfo{};

		create_info.vulkanApiVersion = vk::ApiVersion14;
		// create_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

		create_info.physicalDevice = physical_device;
		create_info.device = device;
		create_info.instance = instance;

		VmaAllocator allocator;
		const auto result = vmaCreateAllocator(&create_info, &allocator);
		if (result != VK_SUCCESS)
			return Error("Create VMA allocator failed", vk::to_string(vk::Result(result)));

		return Allocator(std::make_unique<AllocatorWrapper>(allocator));
	}

	std::expected<Image, Error> Allocator::create_image(
		const vk::ImageCreateInfo& create_info,
		MemoryUsage usage
	) const noexcept
	{
		const VkImageCreateInfo create_info_c = create_info;

		VmaAllocationCreateInfo allocation_info = {};
		allocation_info.usage = static_cast<VmaMemoryUsage>(usage);

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

		return Image(std::make_unique<ImageWrapper>(image, allocation, wrapper->allocator));
	}

	std::expected<Buffer, Error> Allocator::create_buffer(
		const vk::BufferCreateInfo& create_info,
		MemoryUsage usage
	) const noexcept
	{
		const VkBufferCreateInfo create_info_c = create_info;

		VmaAllocationCreateInfo allocation_info = {};
		allocation_info.usage = static_cast<VmaMemoryUsage>(usage);

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

		return Buffer(std::make_unique<BufferWrapper>(buffer, allocation, wrapper->allocator));
	}

	std::expected<void, Error> Buffer::upload(
		std::span<const std::byte> data,
		size_t dst_offset
	) const noexcept
	{
		const auto result = vmaCopyMemoryToAllocation(
			wrapper->allocator,
			data.data(),
			wrapper->allocation,
			dst_offset,
			data.size()
		);
		if (result != VK_SUCCESS)
			return Error("Upload data to buffer failed", vk::to_string(vk::Result(result)));
		return {};
	}

	std::expected<void, Error> Buffer::download(std::span<std::byte> data, size_t src_offset) const noexcept
	{
		const auto result = vmaCopyAllocationToMemory(
			wrapper->allocator,
			wrapper->allocation,
			src_offset,
			data.data(),
			data.size()
		);
		if (result != VK_SUCCESS)
			return Error("Download data from buffer failed", vk::to_string(vk::Result(result)));
		return {};
	}

	AllocatorWrapper::~AllocatorWrapper() noexcept
	{
		vmaDestroyAllocator(allocator);
	}

	ImageWrapper::~ImageWrapper() noexcept
	{
		vmaDestroyImage(allocator, image, allocation);
	}

	BufferWrapper::~BufferWrapper() noexcept
	{
		vmaDestroyBuffer(allocator, buffer, allocation);
	}
}
