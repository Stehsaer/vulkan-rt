#pragma once

#include "common/util/error.hpp"
#include "image.hpp"
#include "image/common.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <mdspan>

namespace image
{
	///
	/// @brief Acquire a 128x128 blue noise image in 16-bit RGBA format. Embedded in the binary.
	///
	/// @return Blue noise image or an error
	///
	[[nodiscard]]
	std::expected<Image<Format::Unorm16, Layout::RGBA>, Error> get_blue_noise() noexcept;

	///
	/// @brief Spatiotemporal blue noise, accessible directly as raw data
	/// @details 64 spatiotemporal blue noise textures, each 128x128 pixels
	///
	/// @note
	/// - Asset is taken from https://github.com/NVIDIA-RTX/STBN/blob/main/Assets/STBN.zip, flattened and
	/// embedded as raw pixels
	/// - See paper: https://arxiv.org/abs/2112.09629
	/// - See article:
	/// https://developer.nvidia.com/blog/rendering-in-real-time-with-spatiotemporal-blue-noise-textures-part-2/
	///
	extern const std::mdspan<const glm::u8vec2, std::extents<uint32_t, 64, 128, 128>> stbn_data;
}
