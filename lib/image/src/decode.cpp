#include "image/impl/decode.hpp"
#include "common/util/error.hpp"

#include <stb_image.h>
#include <webp/decode.h>

namespace image::impl
{
	static std::expected<std::vector<uint8_t>, Error> decode_webp(
		std::span<const std::byte> encoded_data,
		int width,
		int height,
		Layout layout
	) noexcept
	{
		std::vector<uint8_t> decoded_data(width * height * std::to_underlying(layout));

		const uint8_t* decode_result;

		switch (layout)
		{
		case Layout::Grey:
		case Layout::RG:
			return Error("WebP does not support this layout");

		case Layout::RGB:
			decode_result = WebPDecodeRGBInto(
				reinterpret_cast<const uint8_t*>(encoded_data.data()),
				encoded_data.size(),
				decoded_data.data(),
				decoded_data.size(),
				width * 3
			);
			break;

		case Layout::RGBA:
			decode_result = WebPDecodeRGBAInto(
				reinterpret_cast<const uint8_t*>(encoded_data.data()),
				encoded_data.size(),
				decoded_data.data(),
				decoded_data.size(),
				width * 4
			);
			break;

		default:
			return Error("Unexpected layout");
		}

		if (decode_result == nullptr) return Error("Decode WebP image failed");

		return decoded_data;
	}

	template <>
	std::expected<DecodeResult<Format::Unorm8>, Error> decode_img<Format::Unorm8>(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept
	{
		int width, height, channels;
		const int desired_channels = static_cast<int>(layout);

		const bool is_webp =
			WebPGetInfo(
				reinterpret_cast<const uint8_t*>(encoded_data.data()),
				encoded_data.size(),
				&width,
				&height
			)
			!= 0;

		if (is_webp)
		{
			auto result = decode_webp(encoded_data, width, height, layout);
			if (!result) return result.error();

			return DecodeResult<Format::Unorm8>{
				.data = std::move(*result),
				.width = static_cast<uint32_t>(width),
				.height = static_cast<uint32_t>(height)
			};
		}

		stbi_uc* data = stbi_load_from_memory(
			reinterpret_cast<const stbi_uc*>(encoded_data.data()),
			int(encoded_data.size()),
			&width,
			&height,
			&channels,
			desired_channels
		);
		if (data == nullptr) return Error("Decode image failed", stbi_failure_reason());

		auto result = DecodeResult<Format::Unorm8>{
			.data = std::vector<uint8_t>(std::from_range, std::span(data, width * height * desired_channels)),
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height)
		};
		stbi_image_free(data);
		return result;
	}

	template <>
	std::expected<DecodeResult<Format::Unorm16>, Error> decode_img<Format::Unorm16>(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept
	{
		int width, height, channels;
		const int desired_channels = static_cast<int>(layout);

		const bool is_webp =
			WebPGetInfo(
				reinterpret_cast<const uint8_t*>(encoded_data.data()),
				encoded_data.size(),
				&width,
				&height
			)
			!= 0;

		if (is_webp)
		{
			const auto result = decode_webp(encoded_data, width, height, layout);
			if (!result) return result.error();

			return DecodeResult<Format::Unorm16>{
				.data = *result | std::views::transform([](uint8_t byte) -> uint16_t {
					return static_cast<uint16_t>(byte) * 0x0101;
				}) | std::ranges::to<std::vector>(),
				.width = static_cast<uint32_t>(width),
				.height = static_cast<uint32_t>(height)
			};
		}

		stbi_us* data = stbi_load_16_from_memory(
			reinterpret_cast<const stbi_uc*>(encoded_data.data()),
			int(encoded_data.size()),
			&width,
			&height,
			&channels,
			desired_channels
		);
		if (data == nullptr) return Error("Decode image failed", stbi_failure_reason());

		auto result = DecodeResult<Format::Unorm16>{
			.data =
				std::vector<uint16_t>(std::from_range, std::span(data, width * height * desired_channels)),
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height)
		};
		stbi_image_free(data);

		return result;
	}

	template <>
	std::expected<DecodeResult<Format::Float32>, Error> decode_img<Format::Float32>(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept
	{
		int width, height, channels;
		const int desired_channels = static_cast<int>(layout);

		float* data = stbi_loadf_from_memory(
			reinterpret_cast<const stbi_uc*>(encoded_data.data()),
			int(encoded_data.size()),
			&width,
			&height,
			&channels,
			desired_channels
		);
		if (data == nullptr) return Error("Decode image failed", stbi_failure_reason());

		auto result = DecodeResult<Format::Float32>{
			.data = std::vector<float>(std::from_range, std::span(data, width * height * desired_channels)),
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height)
		};
		stbi_image_free(data);

		return result;
	}
}
