#include "image/raw-image.hpp"
#include "common/utility/error.hpp"

#include <stb_image.h>

namespace image::impl
{
	template <>
	std::expected<DecodeResult<Precision::Uint8>, Error> decode_img<Precision::Uint8>(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept
	{
		int width, height, channels;
		const int desired_channels = static_cast<int>(layout);

		stbi_uc* data = stbi_load_from_memory(
			reinterpret_cast<const stbi_uc*>(encoded_data.data()),
			int(encoded_data.size()),
			&width,
			&height,
			&channels,
			desired_channels
		);
		if (data == nullptr) return Error(std::format("Decode image failed: {}", stbi_failure_reason()));

		auto result = DecodeResult<Precision::Uint8>{
			.data = std::vector<uint8_t>(std::from_range, std::span(data, width * height * desired_channels)),
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height)
		};
		stbi_image_free(data);
		return result;
	}

	template <>
	std::expected<DecodeResult<Precision::Uint16>, Error> decode_img<Precision::Uint16>(
		std::span<const std::byte> encoded_data,
		Layout layout
	) noexcept
	{
		int width, height, channels;
		const int desired_channels = static_cast<int>(layout);

		stbi_us* data = stbi_load_16_from_memory(
			reinterpret_cast<const stbi_uc*>(encoded_data.data()),
			int(encoded_data.size()),
			&width,
			&height,
			&channels,
			desired_channels
		);
		if (data == nullptr) return Error(std::format("Decode image failed: {}", stbi_failure_reason()));

		auto result = DecodeResult<Precision::Uint16>{
			.data =
				std::vector<uint16_t>(std::from_range, std::span(data, width * height * desired_channels)),
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height)
		};
		stbi_image_free(data);

		return result;
	}

	template <>
	std::expected<DecodeResult<Precision::Float32>, Error> decode_img<Precision::Float32>(
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
		if (data == nullptr) return Error(std::format("Decode image failed: {}", stbi_failure_reason()));

		auto result = DecodeResult<Precision::Float32>{
			.data = std::vector<float>(std::from_range, std::span(data, width * height * desired_channels)),
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height)
		};
		stbi_image_free(data);

		return result;
	}

}