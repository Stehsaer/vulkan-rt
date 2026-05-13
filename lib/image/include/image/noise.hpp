#pragma once

#include "common/util/error.hpp"
#include "image.hpp"
#include "image/common.hpp"

#include <expected>

namespace image
{
	///
	/// @brief Acquire a 128x128 blue noise image in 16-bit RGBA format. Embedded in the binary.
	///
	/// @return Blue noise image or an error
	///
	[[nodiscard]]
	std::expected<Image<Format::Unorm16, Layout::RGBA>, Error> get_blue_noise() noexcept;
}
