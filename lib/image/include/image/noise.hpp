#pragma once

#include "raw-image.hpp"

namespace image
{
	///
	/// @brief Acquire a 128x128 blue noise image in 16-bit RGBA format. Embedded in the binary.
	///
	/// @return Blue noise image or an error
	///
	[[nodiscard]]
	std::expected<RawImage<Precision::Uint16, Layout::RGBA>, Error> get_blue_noise() noexcept;
}