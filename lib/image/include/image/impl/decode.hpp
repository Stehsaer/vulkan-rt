#pragma once

#include "common/util/error.hpp"
#include "image/common.hpp"

#include <cstddef>
#include <glm/glm.hpp>
#include <span>

namespace image::impl
{
	template <Format T>
	struct DecodeResult
	{
		std::vector<FormatType<T>> data;
		uint32_t width;
		uint32_t height;
	};

	template <Format T>
	[[nodiscard]]
	std::expected<DecodeResult<T>, Error> decode_img(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept;

	template <>
	std::expected<DecodeResult<Format::Unorm8>, Error> decode_img<Format::Unorm8>(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept;

	template <>
	std::expected<DecodeResult<Format::Unorm16>, Error> decode_img<Format::Unorm16>(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept;

	template <>
	std::expected<DecodeResult<Format::Float32>, Error> decode_img<Format::Float32>(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept;
}
