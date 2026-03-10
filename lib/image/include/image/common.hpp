#pragma once

#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <optional>
#include <stb_image_resize2.h>
#include <utility>
#include <variant>
#include <vector>

namespace image
{
	template <typename T>
	constexpr bool indexable_pixel_type_flag = true;

	template <typename T>
	concept IndexablePixel = indexable_pixel_type_flag<T>;

	template <typename T>
	struct Container
	{
		glm::u32vec2 size;
		std::vector<T> data;

		[[nodiscard]]
		auto& operator[](this auto& self, uint32_t x, uint32_t y)
			requires(IndexablePixel<T>)
		{
			assert(x < self.size.x && y < self.size.y);
			return self.data[y * self.size.x + x];
		}

		[[nodiscard]]
		auto& operator[](this auto& self, glm::u32vec2 coord)
			requires(IndexablePixel<T>)
		{
			assert(coord.x < self.size.x && coord.y < self.size.y);
			return self.data[coord.y * self.size.x + coord.x];
		}
	};

	enum class Format
	{
		Unorm8,
		Unorm16,
		Float32
	};

	enum class Layout
	{
		Grey = 1,
		RG = 2,
		RGB = 3,
		RGBA = 4
	};

	namespace encode_format
	{
		struct Png
		{
			uint8_t compression_level = 8;
		};

		struct Jpg
		{
			uint8_t quality = 95;
		};

		struct Bmp
		{};
	}

	using EncodeFormat = std::variant<encode_format::Png, encode_format::Jpg, encode_format::Bmp>;

	namespace detail
	{
		/* STBIR TYPE */

		template <Format T>
		inline constexpr std::optional<stbir_datatype> stbir_type = std::nullopt;

		template <>
		inline constexpr std::optional<stbir_datatype> stbir_type<Format::Unorm8> = STBIR_TYPE_UINT8;

		template <>
		inline constexpr std::optional<stbir_datatype> stbir_type<Format::Unorm16> = STBIR_TYPE_UINT16;

		template <>
		inline constexpr std::optional<stbir_datatype> stbir_type<Format::Float32> = STBIR_TYPE_FLOAT;

		/* STBIR LAYOUT */

		template <Layout L>
		inline constexpr std::optional<stbir_pixel_layout> stbir_layout = std::nullopt;

		template <>
		inline constexpr std::optional<stbir_pixel_layout> stbir_layout<Layout::Grey> = STBIR_1CHANNEL;

		template <>
		inline constexpr std::optional<stbir_pixel_layout> stbir_layout<Layout::RG> = STBIR_2CHANNEL;

		template <>
		inline constexpr std::optional<stbir_pixel_layout> stbir_layout<Layout::RGB> = STBIR_RGB;

		template <>
		inline constexpr std::optional<stbir_pixel_layout> stbir_layout<Layout::RGBA> = STBIR_RGBA;

		/* Maps format to type */

		template <Format T>
		struct FormatTypeImpl
		{
			using type = void;
		};

		template <>
		struct FormatTypeImpl<Format::Unorm8>
		{
			using type = uint8_t;
		};

		template <>
		struct FormatTypeImpl<Format::Unorm16>
		{
			using type = uint16_t;
		};

		template <>
		struct FormatTypeImpl<Format::Float32>
		{
			using type = float;
		};
	}

	template <Format T>
		requires(!std::same_as<typename detail::FormatTypeImpl<T>::type, void>)
	using FormatType = detail::FormatTypeImpl<T>::type;

	///
	/// @brief Raw pixel type with specified format and layout
	///
	/// @tparam P Image format
	/// @tparam L Image layout
	///
	template <Format T, Layout L>
	using Pixel = glm::vec<std::to_underlying(L), typename detail::FormatTypeImpl<T>::type>;

	namespace detail::deduce
	{
		template <typename T>
		consteval std::optional<Format> deduce_format() noexcept
		{
			using ElementType = decltype(std::declval<T>().x);
			if constexpr (std::same_as<ElementType, uint8_t>) return Format::Unorm8;
			if constexpr (std::same_as<ElementType, uint16_t>) return Format::Unorm16;
			if constexpr (std::same_as<ElementType, float>) return Format::Float32;

			return std::nullopt;
		}

		template <typename T>
		consteval std::optional<Layout> deduce_layout() noexcept
		{
			constexpr auto format = deduce_format<T>();
			if constexpr (!format.has_value()) return std::nullopt;

			using ElementType = decltype(std::declval<T>().x);

			constexpr auto ratio = sizeof(T) / sizeof(ElementType);
			if constexpr (ratio < 1 || ratio > 4) return std::nullopt;
			return static_cast<Layout>(ratio);
		}

		template <typename T>
		consteval std::pair<Format, Layout> deduce_image_type() noexcept
		{
			constexpr auto deduced_format = deduce_format<T>();
			static_assert(deduced_format.has_value(), "Can't deduce format");

			constexpr auto deduced_layout = deduce_layout<T>();
			static_assert(deduced_layout.has_value(), "Can't deduce layout");

			constexpr auto format = deduced_format.value();
			constexpr auto layout = deduced_layout.value();

			static_assert(std::convertible_to<T, Pixel<format, layout>>, "T isn't convertible to image type");

			return {format, layout};
		}
	}
}
