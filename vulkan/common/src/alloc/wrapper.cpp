#include "vulkan/common/alloc/wrapper.hpp"

namespace vulkan::impl
{
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
