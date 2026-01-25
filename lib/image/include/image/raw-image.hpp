#pragma once

#include "common/util/error.hpp"
#include "common/util/span-util.hpp"
#include "image/common.hpp"

#include <cstddef>
#include <glm/glm.hpp>
#include <span>
#include <stb_image_resize2.h>
#include <utility>

namespace image
{
	enum class Precision
	{
		Uint8,
		Uint16,
		Float32
	};

	enum class Layout
	{
		Grey = 1,
		RGBA = 4
	};

	namespace detail
	{
		template <Precision P>
		inline constexpr std::optional<stbir_datatype> stbir_type = std::nullopt;

		template <>
		inline constexpr std::optional<stbir_datatype> stbir_type<Precision::Uint8> = STBIR_TYPE_UINT8;

		template <>
		inline constexpr std::optional<stbir_datatype> stbir_type<Precision::Uint16> = STBIR_TYPE_UINT16;

		template <>
		inline constexpr std::optional<stbir_datatype> stbir_type<Precision::Float32> = STBIR_TYPE_FLOAT;

		template <Layout L>
		inline constexpr std::optional<stbir_pixel_layout> stbir_layout = std::nullopt;

		template <>
		inline constexpr std::optional<stbir_pixel_layout> stbir_layout<Layout::Grey> = STBIR_1CHANNEL;

		template <>
		inline constexpr std::optional<stbir_pixel_layout> stbir_layout<Layout::RGBA> = STBIR_RGBA;

		template <Precision P>
		struct PrecisionType
		{
			using type = void;
		};

		template <>
		struct PrecisionType<Precision::Uint8>
		{
			using type = uint8_t;
		};

		template <>
		struct PrecisionType<Precision::Uint16>
		{
			using type = uint16_t;
		};

		template <>
		struct PrecisionType<Precision::Float32>
		{
			using type = float;
		};
	}

	template <Precision P>
	using PrecisionType = detail::PrecisionType<P>::type;

	template <Layout L>
	constexpr size_t LayoutChannels = std::to_underlying(L);

	///
	/// @brief Raw pixel type with specified precision and layout
	///
	/// @tparam P Image precision
	/// @tparam L Image layout
	///
	template <Precision P, Layout L>
	using RawPixel = glm::vec<LayoutChannels<L>, typename detail::PrecisionType<P>::type, glm::defaultp>;

	///
	/// @brief Raw image with specified precision and layout
	///
	/// @tparam P Image precision
	/// @tparam L Image layout
	///
	template <Precision P, Layout L>
	struct RawImage : public Container<RawPixel<P, L>>
	{
		RawImage(const RawImage&) = default;
		RawImage(RawImage&&) = default;
		RawImage& operator=(const RawImage&) = default;
		RawImage& operator=(RawImage&&) = default;

		///
		/// @brief Construct a new Raw Image object filled with 0
		///
		/// @param size Image size
		///
		RawImage(glm::u32vec2 size) noexcept :
			Container<RawPixel<P, L>>{.size = size, .data = std::vector<RawPixel<P, L>>(size.x * size.y)}
		{}

		///
		/// @brief Construct a new Raw Image object filled with a specific value
		///
		/// @param size Image size
		/// @param fill_value Value to fill the image with
		///
		RawImage(glm::u32vec2 size, const RawPixel<P, L>& fill_value) noexcept :
			Container<RawPixel<P, L>>{
				.size = size,
				.data = std::vector<RawPixel<P, L>>(size.x * size.y, fill_value)
			}
		{}

		///
		/// @brief Construct a new Raw Image object from size and data
		/// @warning The size of data must match size.x * size.y, otherwise an assertion will fail
		/// @param size Image size
		/// @param data Image data
		///
		RawImage(glm::u32vec2 size, std::vector<RawPixel<P, L>> data) noexcept :
			Container<RawPixel<P, L>>{.size = size, .data = std::move(data)}
		{
			assert(this->data.size() == size.x * size.y);
		}

		///
		/// @brief Decode an image from encoded data
		///
		/// @param encoded_data Encoded image data
		/// @return Decoded RawImage or Error
		///
		static std::expected<RawImage, Error> decode(std::span<const std::byte> encoded_data) noexcept;

		///
		/// @brief Resize image to new size
		///
		/// @param new_size New image size
		/// @return Resized RawImage
		///
		RawImage resize(glm::u32vec2 new_size) const noexcept;
	};

	/* Implementations */

	namespace impl
	{
		template <Precision P>
		struct DecodeResult
		{
			std::vector<PrecisionType<P>> data;
			uint32_t width;
			uint32_t height;
		};

		template <Precision P>
		std::expected<DecodeResult<P>, Error> decode_img(
			std::span<const std::byte> encoded_data,
			Layout layout
		) noexcept;

		template <>
		std::expected<DecodeResult<Precision::Uint8>, Error> decode_img<Precision::Uint8>(
			std::span<const std::byte> encoded_data,
			Layout layout
		) noexcept;

		template <>
		std::expected<DecodeResult<Precision::Uint16>, Error> decode_img<Precision::Uint16>(
			std::span<const std::byte> encoded_data,
			Layout layout
		) noexcept;

		template <>
		std::expected<DecodeResult<Precision::Float32>, Error> decode_img<Precision::Float32>(
			std::span<const std::byte> encoded_data,
			Layout layout
		) noexcept;
	}

	template <Precision P, Layout L>
	RawImage<P, L> RawImage<P, L>::resize(glm::u32vec2 new_size) const noexcept
	{
		RawImage<P, L> resized_image(new_size);

		stbir_resize(
			this->data.data(),
			int(this->size.x),
			int(this->size.y),
			sizeof(RawPixel<P, L>) * this->size.x,
			resized_image.data.data(),
			int(new_size.x),
			int(new_size.y),
			sizeof(RawPixel<P, L>) * new_size.x,
			*detail::stbir_layout<L>,
			*detail::stbir_type<P>,
			STBIR_EDGE_CLAMP,
			STBIR_FILTER_MITCHELL
		);

		return resized_image;
	}

	template <Precision P, Layout L>
	std::expected<RawImage<P, L>, Error> RawImage<P, L>::decode(
		std::span<const std::byte> encoded_data
	) noexcept
	{
		const auto result = impl::decode_img<P>(encoded_data, L);
		if (!result) return result.error();

		std::vector<RawPixel<P, L>> decoded_data(result->width * result->height);
		std::ranges::copy(util::as_bytes(result->data), util::as_writable_bytes(decoded_data).begin());

		return RawImage<P, L>(glm::u32vec2(result->width, result->height), std::move(decoded_data));
	}
}