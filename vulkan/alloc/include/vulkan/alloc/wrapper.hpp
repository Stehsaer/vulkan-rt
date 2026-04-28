#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace vulkan::impl
{
	struct AllocatorWrapper
	{
		VmaAllocator allocator;

		AllocatorWrapper(VmaAllocator allocator) :
			allocator(allocator)
		{}

		~AllocatorWrapper() noexcept;
	};

	struct ImageWrapper
	{
		vk::Image image;
		VmaAllocation allocation;
		VmaAllocator allocator;

		ImageWrapper(vk::Image image, VmaAllocation allocation, VmaAllocator allocator) :
			image(image),
			allocation(allocation),
			allocator(allocator)
		{}

		~ImageWrapper() noexcept;
	};

	struct BufferWrapper
	{
		vk::Buffer buffer;
		VmaAllocation allocation;
		VmaAllocator allocator;

		BufferWrapper(vk::Buffer buffer, VmaAllocation allocation, VmaAllocator allocator) :
			buffer(buffer),
			allocation(allocation),
			allocator(allocator)
		{}

		~BufferWrapper() noexcept;
	};
}
