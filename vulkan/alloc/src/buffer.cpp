#include "vulkan/alloc/buffer.hpp"

namespace vulkan
{
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
}
