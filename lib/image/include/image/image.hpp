#pragma once

#include "common/util/error.hpp"
#include "common/util/span.hpp"
#include "image/common.hpp"
#include "image/impl/decode.hpp"

#include <bit>
#include <cstddef>
#include <glm/glm.hpp>
#include <span>
#include <stb_image_resize2.h>
#include <utility>

namespace image
{
	// Concept for constraining pixel mapping function
	template <typename MapFunc, Format T, Layout L, Format Ty, Layout Ly>
	concept PixelMapFunc = std::invocable<MapFunc, Pixel<T, L>>
		&& std::convertible_to<std::invoke_result_t<MapFunc, Pixel<T, L>>, Pixel<Ty, Ly>>;

	///
	/// @brief `log2` of the smaller nearby power-of-two value
	/// @details
	/// Example:
	/// - `log2(6) = log2(4) = 2`
	/// - `log2(511) = log2(256) = 8`
	///
	uint32_t log2(uint32_t val) noexcept;

	///
	/// @brief Raw image with specified format and layout
	///
	/// @tparam P Image format
	/// @tparam L Image layout
	///
	template <Format T, Layout L>
	struct Image : public Container<Pixel<T, L>>
	{
		/*===== Types =====*/

		static constexpr Format format = T;
		static constexpr Layout layout = L;

		/*===== CONSTRUCTORS =====*/

		///
		/// @brief Construct a new image filled with 0
		///
		/// @param size Image size
		///
		Image(glm::u32vec2 size) noexcept :
			Container<Pixel<T, L>>{.size = size, .data = std::vector<Pixel<T, L>>(size.x * size.y)}
		{
			assert(size.x > 0 && size.y > 0);
		}

		///
		/// @brief Construct a new image filled with a specific value
		///
		/// @param size Image size
		/// @param fill_value Value to fill the image with
		///
		Image(glm::u32vec2 size, const Pixel<T, L>& fill_value) noexcept :
			Container<Pixel<T, L>>{
				.size = size,
				.data = std::vector<Pixel<T, L>>(size.x * size.y, fill_value)
			}
		{
			assert(size.x > 0 && size.y > 0);
		}

		///
		/// @brief Construct a new image from size and data
		/// @warning The size of data must match `size.x * size.y`, otherwise an assertion will fail
		///
		/// @param size Image size
		/// @param data Image data
		///
		Image(glm::u32vec2 size, std::vector<Pixel<T, L>> data) noexcept :
			Container<Pixel<T, L>>{.size = size, .data = std::move(data)}
		{
			assert(size.x > 0 && size.y > 0);
			assert(this->data.size() == size.x * size.y);
		}

		///
		/// @brief Construct a new image from size and range
		/// @warning The size of data must match `size.x * size.y`, otherwise an assertion will fail
		///
		/// @param size Image size
		/// @param range Image data range
		///
		template <std::ranges::input_range Range>
		Image(glm::u32vec2 size, Range&& range) noexcept :
			Container<Pixel<T, L>>{
				.size = size,
				.data = std::vector<Pixel<T, L>>(std::from_range, std::forward<Range&&>(range))
			}
		{
			assert(size.x > 0 && size.y > 0);
			assert(this->data.size() == size.x * size.y);
		}

		/*===== Functions =====*/

		///
		/// @brief Decode an image from encoded data
		///
		/// @param encoded_data Encoded image data
		/// @return Decoded Image or Error
		///
		[[nodiscard]]
		static std::expected<Image, Error> decode(std::span<const std::byte> encoded_data) noexcept
		{
			const auto result = impl::decode_img<T>(encoded_data, L);
			if (!result) return result.error();

			std::vector<Pixel<T, L>> decoded_data(result->width * result->height);
			std::ranges::copy(util::as_bytes(result->data), util::as_writable_bytes(decoded_data).begin());

			return Image<T, L>(glm::u32vec2(result->width, result->height), std::move(decoded_data));
		}

		///
		/// @brief Resize image to new size
		///
		/// @param new_size New image size
		/// @param filter Resize filter
		/// @return Resized Image
		///
		[[nodiscard]]
		Image resize(glm::u32vec2 new_size, stbir_filter filter = STBIR_FILTER_MITCHELL) const noexcept
		{
			Image<T, L> resized_image(new_size);

			stbir_resize(
				this->data.data(),
				int(this->size.x),
				int(this->size.y),
				sizeof(Pixel<T, L>) * this->size.x,
				resized_image.data.data(),
				int(new_size.x),
				int(new_size.y),
				sizeof(Pixel<T, L>) * new_size.x,
				*detail::stbir_layout<L>,
				*detail::stbir_type<T>,
				STBIR_EDGE_CLAMP,
				filter
			);

			return resized_image;
		}

		///
		/// @brief Map every pixel through a function and produce a new image
		/// @note The resulting image type is automatically deduced by the return type of `MapFunc`
		///
		/// @tparam MapFunc Mapping function
		/// @return Mapped image
		///
		template <typename MapFunc>
		[[nodiscard]]
		auto map(MapFunc map_func) const noexcept
		{
			constexpr auto deduced_type =
				detail::deduce::deduce_image_type<std::invoke_result_t<MapFunc, Pixel<T, L>>>();

			return Image<deduced_type.first, deduced_type.second>(
				this->size,
				this->data | std::views::transform(map_func)
			);
		}

		///
		/// @brief Tell if the image is POT (Power-of-two)
		///
		/// @return `true` if is POT
		///
		[[nodiscard]]
		bool is_pot() const noexcept
		{
			return std::popcount(this->size.x) == 1 && std::popcount(this->size.y) == 1;
		}

		///
		/// @brief Resize to POT
		///
		/// @param larger_size Set to `true` to resize as larger POT size (e.g. `1536 -> 2048`), set to
		/// `false` to resize as smaller POT size (e.g. `1536 -> 1024`)
		/// @param filter Filter used to resize
		/// @return Resized `Image`
		///
		[[nodiscard]]
		Image resize_to_pot(bool larger_size, stbir_filter filter = STBIR_FILTER_MITCHELL) const noexcept
		{
			const uint32_t new_size_x =
				this->size.x == 1 ? 1 : glm::exp2(log2(this->size.x - 1u) + (larger_size ? 1u : 0u));
			const uint32_t new_size_y =
				this->size.y == 1 ? 1 : glm::exp2(log2(this->size.y - 1u) + (larger_size ? 1u : 0u));

			return this->resize({new_size_x, new_size_y}, filter);
		}

		///
		/// @brief Generate mipmap. If image is NPOT, returns mipmap with only 1 level
		///
		/// @param min_size_log `log2` of the minimum size wanted. e.g. to limit minimum size of 4, set
		/// `min_size_log=2`
		/// @return Mipmap chain
		///
		[[nodiscard]]
		std::vector<Image> generate_mipmap(uint32_t min_size_log) const noexcept
		{
			std::vector<image::Image<T, L>> results = {*this};
			if (!this->is_pot()) return results;

			const auto min_dim = glm::min(this->size.x, this->size.y);
			const auto clamped_min_log2_size = std::min(log2(min_dim), min_size_log);

			for (const auto _ : std::views::iota(0u, log2(min_dim) - clamped_min_log2_size))
				results.emplace_back(results.back().resize(results.back().size / 2u, STBIR_FILTER_BOX));

			return results;
		}

		///
		/// @brief Generate mipmap. If image is NPOT, scale to larger POT and generate mipmap
		///
		/// @param min_size_log `log2` of the minimum size wanted. e.g. to limit minimum size of 4, set
		/// `min_log2_size=2`
		/// @return Mipmap chain
		///
		[[nodiscard]]
		std::vector<Image> resize_and_generate_mipmap(uint32_t min_size_log) const noexcept
		{
			if (this->is_pot())
				return this->generate_mipmap(min_size_log);
			else
				return this->resize_to_pot().generate_mipmap(min_size_log);
		}

		/*===== Copy/Move =====*/

		Image(const Image&) = default;
		Image(Image&&) = default;
		Image& operator=(const Image&) = default;
		Image& operator=(Image&&) = default;
	};

	///
	/// @brief Check if the encoded image data is in 16-bit format, without fully decoding it.
	///
	/// @param encoded_data Encoded image data
	///
	/// @retval true if the image is in 16-bit format
	/// @retval false if the image is in 8-bit format OR if the format cannot be determined (e.g. unsupported
	/// format)
	///
	bool encoded_data_is_16bit(std::span<const std::byte> encoded_data) noexcept;
}
