#include <doctest/doctest.h>
#include <span>

#include "common/test-macro.hpp"
#include "image/bc-image.hpp"

extern const std::byte _binary_checker_png_start;
extern const std::byte _binary_checker_png_end;

using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

TEST_CASE("BC3")
{
	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	EXPECT_SUCCESS(decoded_image_result);

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_VEC2_EQ(decoded_image.size, 256, 256);

	auto bc3_image_result = image::BlockCompressedImage::encode(decoded_image, image::BCnFormat::BC3);
	EXPECT_SUCCESS(bc3_image_result);
	CHECK_EQ(bc3_image_result->format, image::BCnFormat::BC3);
	CHECK_VEC2_EQ(bc3_image_result->block_dim, 64, 64);
}

TEST_CASE("BC5")
{
	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	EXPECT_SUCCESS(decoded_image_result);

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_VEC2_EQ(decoded_image.size, 256, 256);

	auto bc5_image_result = image::BlockCompressedImage::encode(decoded_image, image::BCnFormat::BC5);
	EXPECT_SUCCESS(bc5_image_result);
	CHECK_EQ(bc5_image_result->format, image::BCnFormat::BC5);
	CHECK_VEC2_EQ(bc5_image_result->block_dim, 64, 64);
}

TEST_CASE("BC7")
{
	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	EXPECT_SUCCESS(decoded_image_result);

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(256, 256));

	auto bc7_image_result = image::BlockCompressedImage::encode(decoded_image, image::BCnFormat::BC7);
	EXPECT_SUCCESS(bc7_image_result);
	CHECK_EQ(bc7_image_result->format, image::BCnFormat::BC7);
	CHECK_VEC2_EQ(bc7_image_result->block_dim, 64, 64);
}

TEST_CASE("Invalid dimension")
{
	const ImageType invalid_image(glm::u32vec2(255, 255));

	auto bc3_image_result = image::BlockCompressedImage::encode(invalid_image, image::BCnFormat::BC3);
	EXPECT_FAIL(bc3_image_result);

	auto bc5_image_result = image::BlockCompressedImage::encode(invalid_image, image::BCnFormat::BC5);
	EXPECT_FAIL(bc5_image_result);

	auto bc7_image_result = image::BlockCompressedImage::encode(invalid_image, image::BCnFormat::BC7);
	EXPECT_FAIL(bc7_image_result);
}
