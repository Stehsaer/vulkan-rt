#include "image/image.hpp"

#include <stb_image.h>

namespace image
{
	uint32_t log2(uint32_t val) noexcept
	{
		assert(val > 0);
		return 31 - std::countl_zero(val);
	};

	bool encoded_data_is_16bit(std::span<const std::byte> encoded_data) noexcept
	{
		const auto result = stbi_is_16_bit_from_memory(
			reinterpret_cast<const stbi_uc*>(encoded_data.data()),
			int(encoded_data.size())
		);

		return result != 0;
	}
}
