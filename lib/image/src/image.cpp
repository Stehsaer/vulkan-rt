#include "image/image.hpp"

#include <stb_image.h>
#include <webp/decode.h>

namespace image
{
	uint32_t log2(uint32_t val) noexcept
	{
		ASSUME(val > 0);
		return 31 - std::countl_zero(val);
	};

	bool encoded_data_is_16bit(std::span<const std::byte> encoded_data) noexcept
	{
		// Check webp
		{
			const auto result = WebPGetInfo(
				reinterpret_cast<const uint8_t*>(encoded_data.data()),
				encoded_data.size(),
				nullptr,
				nullptr
			);

			if (result != 0) return false;  // WebP does not support 16-bit format
		}

		const auto result = stbi_is_16_bit_from_memory(
			reinterpret_cast<const stbi_uc*>(encoded_data.data()),
			int(encoded_data.size())
		);

		return result != 0;
	}
}
