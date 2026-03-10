#pragma once

#include "common/util/error.hpp"
#include "image/common.hpp"

#include <span>

namespace image::impl
{
	[[nodiscard]]
	std::expected<std::vector<std::byte>, Error> encode(
		EncodeFormat format,
		glm::u32vec2 size,
		Layout layout,
		std::span<const std::byte> data
	) noexcept;
}
