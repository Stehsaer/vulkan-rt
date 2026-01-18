#include <doctest/doctest.h>
#include <span>

#include "image/bc-image.hpp"

extern const std::byte _binary_checker_png_start;
extern const std::byte _binary_checker_png_end;

using ImageType = image::RawImage<image::Precision::Uint8, image::Layout::RGBA>;

TEST_CASE("BC3")
{
	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(256, 256));

	auto bc3_image_result = image::BlockCompressedImage::encode(decoded_image, image::BCnFormat::BC3);
	REQUIRE(bc3_image_result.has_value());
	CHECK(bc3_image_result->format == image::BCnFormat::BC3);
	CHECK(bc3_image_result->block_dim == glm::u32vec2(64, 64));
}

TEST_CASE("BC5")
{
	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(256, 256));

	auto bc5_image_result = image::BlockCompressedImage::encode(decoded_image, image::BCnFormat::BC5);
	REQUIRE(bc5_image_result.has_value());
	CHECK(bc5_image_result->format == image::BCnFormat::BC5);
	CHECK(bc5_image_result->block_dim == glm::u32vec2(64, 64));
}

TEST_CASE("BC7")
{
	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(256, 256));

	auto bc7_image_result = image::BlockCompressedImage::encode(decoded_image, image::BCnFormat::BC7);
	REQUIRE(bc7_image_result.has_value());
	CHECK(bc7_image_result->format == image::BCnFormat::BC7);
	CHECK(bc7_image_result->block_dim == glm::u32vec2(64, 64));
}

TEST_CASE("Invalid dimension")
{
	const ImageType invalid_image(glm::u32vec2(255, 255));

	auto bc3_image_result = image::BlockCompressedImage::encode(invalid_image, image::BCnFormat::BC3);
	REQUIRE_FALSE(bc3_image_result.has_value());

	auto bc5_image_result = image::BlockCompressedImage::encode(invalid_image, image::BCnFormat::BC5);
	REQUIRE_FALSE(bc5_image_result.has_value());

	auto bc7_image_result = image::BlockCompressedImage::encode(invalid_image, image::BCnFormat::BC7);
	REQUIRE_FALSE(bc7_image_result.has_value());
}