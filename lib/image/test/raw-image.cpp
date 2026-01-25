#include <doctest/doctest.h>
#include <span>

#include "common/util/span-util.hpp"
#include "image/raw-image.hpp"

extern const std::byte _binary_load8_png_start;
extern const std::byte _binary_load8_png_end;

extern const std::byte _binary_load16_png_start;
extern const std::byte _binary_load16_png_end;

extern const std::byte _binary_checker_png_start;
extern const std::byte _binary_checker_png_end;

TEST_CASE("Indexing")
{
	using ImageType = image::RawImage<image::Precision::Uint8, image::Layout::RGBA>;

	SUBCASE("Read")
	{
		ImageType image(
			glm::u32vec2(2, 2),
			std::vector<image::RawPixel<image::Precision::Uint8, image::Layout::RGBA>>{
				glm::u8vec4(255, 0, 0, 255),
				glm::u8vec4(0, 255, 0, 255),
				glm::u8vec4(0, 0, 255, 255),
				glm::u8vec4(255, 255, 255, 255)
			}
		);

		CHECK_EQ(image[0, 0], glm::u8vec4(255, 0, 0, 255));
		CHECK_EQ(image[1, 0], glm::u8vec4(0, 255, 0, 255));
		CHECK_EQ(image[0, 1], glm::u8vec4(0, 0, 255, 255));
		CHECK_EQ(image[1, 1], glm::u8vec4(255, 255, 255, 255));
	}

	SUBCASE("Write")
	{
		ImageType image(glm::u32vec2(2, 2));

		image[0, 0] = glm::u8vec4(255, 0, 0, 255);
		image[1, 0] = glm::u8vec4(0, 255, 0, 255);
		image[0, 1] = glm::u8vec4(0, 0, 255, 255);
		image[1, 1] = glm::u8vec4(255, 255, 255, 255);

		CHECK_EQ(image[0, 0], glm::u8vec4(255, 0, 0, 255));
		CHECK_EQ(image[1, 0], glm::u8vec4(0, 255, 0, 255));
		CHECK_EQ(image[0, 1], glm::u8vec4(0, 0, 255, 255));
		CHECK_EQ(image[1, 1], glm::u8vec4(255, 255, 255, 255));
	}
}

TEST_CASE("Load 8bit RGBA")
{
	using ImageType = image::RawImage<image::Precision::Uint8, image::Layout::RGBA>;

	const std::span<const std::byte> encoded_data(&_binary_load8_png_start, &_binary_load8_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(2, 2));

	CHECK_EQ(decoded_image[0, 0], glm::u8vec4(255, 0, 0, 255));
	CHECK_EQ(decoded_image[1, 0], glm::u8vec4(0, 255, 0, 255));
	CHECK_EQ(decoded_image[0, 1], glm::u8vec4(0, 0, 255, 255));
	CHECK_EQ(decoded_image[1, 1], glm::u8vec4(255, 255, 255, 255));
}

TEST_CASE("Load 16bit RGBA")
{
	using ImageType = image::RawImage<image::Precision::Uint16, image::Layout::RGBA>;

	const std::span<const std::byte> encoded_data(&_binary_load16_png_start, &_binary_load16_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(2, 2));

	CHECK_EQ(decoded_image[0, 0], glm::u16vec4(65535, 0, 0, 65535));
	CHECK_EQ(decoded_image[1, 0], glm::u16vec4(0, 65535, 0, 65535));
	CHECK_EQ(decoded_image[0, 1], glm::u16vec4(0, 0, 65535, 65535));
	CHECK_EQ(decoded_image[1, 1], glm::u16vec4(65535, 65535, 65535, 65535));
}

TEST_CASE("Load large image")
{
	using ImageType = image::RawImage<image::Precision::Uint8, image::Layout::RGBA>;

	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(256, 256));
}

TEST_CASE("Load large image grey")
{
	using ImageType = image::RawImage<image::Precision::Uint8, image::Layout::Grey>;

	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(256, 256));
}

TEST_CASE("Invalid Image")
{
	using ImageType = image::RawImage<image::Precision::Uint8, image::Layout::RGBA>;

	const std::vector<uint8_t> invalid_data = {0x00, 0x11, 0x22, 0x33, 0x44};
	auto decoded_image_result = ImageType::decode(util::as_bytes(invalid_data));
	REQUIRE_FALSE(decoded_image_result.has_value());
}

TEST_CASE("Image Resize")
{
	using ImageType = image::RawImage<image::Precision::Uint8, image::Layout::RGBA>;

	const std::span<const std::byte> encoded_data(&_binary_load8_png_start, &_binary_load8_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(2, 2));

	const auto resized_image = decoded_image.resize(glm::u32vec2(4, 4));
	REQUIRE_EQ(resized_image.size, glm::u32vec2(4, 4));

	CHECK_EQ(resized_image[0, 0], glm::u8vec4(255, 0, 0, 255));
	CHECK_EQ(resized_image[3, 0], glm::u8vec4(0, 255, 0, 255));
	CHECK_EQ(resized_image[0, 3], glm::u8vec4(0, 0, 255, 255));
	CHECK_EQ(resized_image[3, 3], glm::u8vec4(255, 255, 255, 255));
}

TEST_CASE("Image Resize Big")
{
	using ImageType = image::RawImage<image::Precision::Uint8, image::Layout::RGBA>;

	const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

	auto decoded_image_result = ImageType::decode(encoded_data);
	REQUIRE(decoded_image_result.has_value());

	const auto decoded_image = std::move(decoded_image_result.value());
	REQUIRE_EQ(decoded_image.size, glm::u32vec2(256, 256));

	const auto resized_image = decoded_image.resize(glm::u32vec2(128, 128));
	REQUIRE_EQ(resized_image.size, glm::u32vec2(128, 128));
}