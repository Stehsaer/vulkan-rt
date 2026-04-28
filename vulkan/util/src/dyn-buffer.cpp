#include "vulkan/util/dyn-buffer.hpp"

namespace vulkan
{
	std::expected<void, Error> DynBuffer::resize(
		const vulkan::DeviceContext& context,
		size_t new_size
	) noexcept
	{
		// Note: exact power of 2 will be rounded up to 3/4 of next power of 2
		const auto determine_size = [](size_t requested_size) {
			const size_t lowest_bigger_pow2 = std::bit_ceil(requested_size + 1);

			if (lowest_bigger_pow2 * 3 / 4 >= requested_size)
				return lowest_bigger_pow2 * 3 / 4;
			else
				return lowest_bigger_pow2;
		};

		const auto clamped_size = std::max(new_size, MIN_SIZE);

		if (clamped_size > MAX_SIZE)
		{
			buffer.reset();
			capacity = 0;
			size = 0;
			shrink_counter = 0;
			return Error(
				"Requested size exceeds maximum allowed size",
				std::format("Requesting {} bytes, exceeds maximum of {} bytes", clamped_size, MAX_SIZE)
			);
		}

		/* Not enough capacity */
		if (clamped_size > capacity || !buffer.has_value())
		{
			const auto determined_size = determine_size(clamped_size);

			auto new_buffer_result = context.allocator.create_buffer(
				vk::BufferCreateInfo{.size = determined_size, .usage = usage},
				memory_usage
			);
			if (!new_buffer_result)
			{
				buffer.reset();
				capacity = 0;
				size = 0;
				shrink_counter = 0;
				return new_buffer_result.error().forward("Create buffer failed");
			}

			buffer = std::move(*new_buffer_result);
			capacity = determined_size;
			size = new_size;
			shrink_counter = 0;

			return {};
		}

		// Note:
		// popcount == 1 => power of 2
		// popcount == 2 => power of 2 * (3/2)
		// Eg. popcount(1536) == 2, popcount(2048) == popcount(1024) == 1
		const auto oversize_thres = (std::popcount(capacity) == 1) ? (capacity * 3 / 4) : (capacity * 2 / 3);
		const bool oversized = clamped_size < oversize_thres;

		if (oversized)
			shrink_counter++;
		else
			shrink_counter = (shrink_counter < FIT_DELTA) ? 0 : shrink_counter - FIT_DELTA;

		if (shrink_counter >= SHRINK_COUNTER_THRES)
		{
			const auto determined_size = determine_size(clamped_size);

			auto new_buffer_result = context.allocator.create_buffer(
				vk::BufferCreateInfo{.size = determined_size, .usage = usage},
				memory_usage
			);
			if (!new_buffer_result)
			{
				buffer.reset();
				capacity = 0;
				size = 0;
				shrink_counter = 0;
				return new_buffer_result.error().forward("Create buffer failed");
			}

			buffer = std::move(*new_buffer_result);
			capacity = determined_size;
			size = new_size;
			shrink_counter = 0;

			return {};
		}

		return {};
	}
}
