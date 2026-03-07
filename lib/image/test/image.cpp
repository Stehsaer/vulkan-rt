#include <doctest/doctest.h>
#include <span>

#include "common/test-macro.hpp"
#include "common/util/span.hpp"
#include "image/image.hpp"

extern const std::byte _binary_load8_png_start;
extern const std::byte _binary_load8_png_end;

extern const std::byte _binary_load16_png_start;
extern const std::byte _binary_load16_png_end;

extern const std::byte _binary_checker_png_start;
extern const std::byte _binary_checker_png_end;

TEST_SUITE("Index")
{
	using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

	TEST_CASE("Read")
	{
		ImageType image(
			glm::u32vec2(2, 2),
			std::vector<image::Pixel<image::Format::Unorm8, image::Layout::RGBA>>{
				glm::u8vec4(255, 0, 0, 255),
				glm::u8vec4(0, 255, 0, 255),
				glm::u8vec4(0, 0, 255, 255),
				glm::u8vec4(255, 255, 255, 255)
			}
		);

		CHECK_VEC4_EQ((image[0, 0]), 255, 0, 0, 255);
		CHECK_VEC4_EQ((image[1, 0]), 0, 255, 0, 255);
		CHECK_VEC4_EQ((image[0, 1]), 0, 0, 255, 255);
		CHECK_VEC4_EQ((image[1, 1]), 255, 255, 255, 255);
	}

	TEST_CASE("Write")
	{
		ImageType image(glm::u32vec2(2, 2));

		image[0, 0] = glm::u8vec4(255, 0, 0, 255);
		image[1, 0] = glm::u8vec4(0, 255, 0, 255);
		image[0, 1] = glm::u8vec4(0, 0, 255, 255);
		image[1, 1] = glm::u8vec4(255, 255, 255, 255);

		CHECK_VEC4_EQ((image[0, 0]), 255, 0, 0, 255);
		CHECK_VEC4_EQ((image[1, 0]), 0, 255, 0, 255);
		CHECK_VEC4_EQ((image[0, 1]), 0, 0, 255, 255);
		CHECK_VEC4_EQ((image[1, 1]), 255, 255, 255, 255);
	}
}

TEST_SUITE("Load")
{
	TEST_CASE("Load 8bit RGBA")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		const std::span<const std::byte> encoded_data(&_binary_load8_png_start, &_binary_load8_png_end);

		auto decoded_image_result = ImageType::decode(encoded_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 2, 2);

		CHECK_VEC4_EQ((decoded_image[0, 0]), 255, 0, 0, 255);
		CHECK_VEC4_EQ((decoded_image[1, 0]), 0, 255, 0, 255);
		CHECK_VEC4_EQ((decoded_image[0, 1]), 0, 0, 255, 255);
		CHECK_VEC4_EQ((decoded_image[1, 1]), 255, 255, 255, 255);
	}

	TEST_CASE("Load 16bit RGBA")
	{
		using ImageType = image::Image<image::Format::Unorm16, image::Layout::RGBA>;

		const std::span<const std::byte> encoded_data(&_binary_load16_png_start, &_binary_load16_png_end);

		auto decoded_image_result = ImageType::decode(encoded_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 2, 2);

		CHECK_VEC4_EQ((decoded_image[0, 0]), 65535, 0, 0, 65535);
		CHECK_VEC4_EQ((decoded_image[1, 0]), 0, 65535, 0, 65535);
		CHECK_VEC4_EQ((decoded_image[0, 1]), 0, 0, 65535, 65535);
		CHECK_VEC4_EQ((decoded_image[1, 1]), 65535, 65535, 65535, 65535);
	}

	TEST_CASE("Load large image")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

		auto decoded_image_result = ImageType::decode(encoded_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 256, 256);
	}

	TEST_CASE("Load large image grey")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::Grey>;

		const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

		auto decoded_image_result = ImageType::decode(encoded_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 256, 256);
	}

	TEST_CASE("Invalid Image")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		const std::vector<uint8_t> invalid_data = {0x00, 0x11, 0x22, 0x33, 0x44};
		auto decoded_image_result = ImageType::decode(util::as_bytes(invalid_data));
		EXPECT_FAIL(decoded_image_result);
	}
}

TEST_SUITE("Resize")
{
	TEST_CASE("Normal")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		const std::span<const std::byte> encoded_data(&_binary_load8_png_start, &_binary_load8_png_end);

		auto decoded_image_result = ImageType::decode(encoded_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 2, 2);

		const auto resized_image = decoded_image.resize(glm::u32vec2(4, 4));
		REQUIRE_VEC2_EQ(resized_image.size, 4, 4);

		CHECK_VEC4_EQ((resized_image[0, 0]), 255, 0, 0, 255);
		CHECK_VEC4_EQ((resized_image[3, 0]), 0, 255, 0, 255);
		CHECK_VEC4_EQ((resized_image[0, 3]), 0, 0, 255, 255);
		CHECK_VEC4_EQ((resized_image[3, 3]), 255, 255, 255, 255);
	}

	TEST_CASE("Big")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		const std::span<const std::byte> encoded_data(&_binary_checker_png_start, &_binary_checker_png_end);

		auto decoded_image_result = ImageType::decode(encoded_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 256, 256);

		const auto resized_image = decoded_image.resize(glm::u32vec2(128, 128));
		REQUIRE_VEC2_EQ(resized_image.size, 128, 128);
	}
}

TEST_CASE("Map")
{
	SUBCASE("Same Type")
	{
		const auto image_data = std::to_array<glm::u8vec4>({
			{0,  1,  2,  3 },
			{4,  5,  6,  7 },
			{8,  9,  10, 11},
			{12, 13, 14, 15}
		});

		const auto img = image::Image<image::Format::Unorm8, image::Layout::RGBA>({2, 2}, image_data);
		const auto mapped_image = img.map([](glm::u8vec4 value) { return value * uint8_t(2); });

		CHECK_VEC4_EQ((mapped_image[0, 0]), 0, 2, 4, 6);
		CHECK_VEC4_EQ((mapped_image[1, 0]), 8, 10, 12, 14);
		CHECK_VEC4_EQ((mapped_image[0, 1]), 16, 18, 20, 22);
		CHECK_VEC4_EQ((mapped_image[1, 1]), 24, 26, 28, 30);
	}

	SUBCASE("Different Type")
	{
		const auto image_data =
			std::to_array<glm::u8vec4>({glm::u8vec4(0), glm::u8vec4(1), glm::u8vec4(2), glm::u8vec4(3)});

		const auto img = image::Image<image::Format::Unorm8, image::Layout::RGBA>({2, 2}, image_data);
		const auto mapped_image =
			img.map([](glm::u8vec4 value) { return glm::f32vec2(value.x, value.y) * 2.0f; });

		static_assert(decltype(mapped_image)::format == image::Format::Float32);
		static_assert(decltype(mapped_image)::layout == image::Layout::RG);

		CHECK_VEC2_EQ((mapped_image[0, 0]), doctest::Approx(0.0), doctest::Approx(0.0));
		CHECK_VEC2_EQ((mapped_image[1, 0]), doctest::Approx(2.0), doctest::Approx(2.0));
		CHECK_VEC2_EQ((mapped_image[0, 1]), doctest::Approx(4.0), doctest::Approx(4.0));
		CHECK_VEC2_EQ((mapped_image[1, 1]), doctest::Approx(6.0), doctest::Approx(6.0));
	}
}

TEST_CASE("Is POT")
{
	CHECK(image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(1, 4)).is_pot());
	CHECK_FALSE(image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(1, 3)).is_pot());
	CHECK_FALSE(image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(13, 1024)).is_pot());
	CHECK(image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(512, 128)).is_pot());
}

TEST_CASE("Resize to POT")
{
	CHECK_VEC2_EQ(
		(image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(123, 456))
			 .resize_to_pot(true)
			 .size),
		128,
		512
	);

	CHECK_VEC2_EQ(
		(image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(123, 456))
			 .resize_to_pot(false)
			 .size),
		64,
		256
	);

	CHECK_VEC2_EQ(
		(image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(1, 1))
			 .resize_to_pot(true)
			 .size),
		1,
		1
	);
}

TEST_SUITE("Generate Mipmap")
{
	TEST_CASE("Generate, no resize")
	{
		SUBCASE("POT")
		{
			const auto img = image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(512, 512));

			// Normal
			const auto result1 = img.generate_mipmap(0);
			REQUIRE_EQ(result1.size(), 10);
			CHECK_VEC2_EQ(result1[0].size, 512, 512);
			CHECK_VEC2_EQ(result1[9].size, 1, 1);

			// Normal, min 4x4
			const auto result2 = img.generate_mipmap(2);
			REQUIRE_EQ(result2.size(), 8);
			CHECK_VEC2_EQ(result2[0].size, 512, 512);
			CHECK_VEC2_EQ(result2[7].size, 4, 4);

			// min_log2_size too large, auto-clamped
			const auto result3 = img.generate_mipmap(10);
			REQUIRE_EQ(result3.size(), 1);
			CHECK_VEC2_EQ(result2[0].size, 512, 512);
		}

		SUBCASE("NPOT")
		{
			const auto img = image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(511, 512));

			const auto result = img.generate_mipmap(0);
			REQUIRE_EQ(result.size(), 1);
			CHECK_VEC2_EQ(result[0].size, 511, 512);
		}
	}

	TEST_CASE("Generate, auto-resize")
	{
		SUBCASE("POT") {}

		SUBCASE("NPOT") {}
	}
}

TEST_CASE("Is 16 bit")
{
	const std::span<const std::byte> encoded_data_unorm8(&_binary_load8_png_start, &_binary_load8_png_end);
	const std::span<const std::byte> encoded_data_unorm16(&_binary_load16_png_start, &_binary_load16_png_end);

	const auto is_16bit_result_8bit = image::encoded_data_is_16bit(encoded_data_unorm8);
	const auto is_16bit_result_16bit = image::encoded_data_is_16bit(encoded_data_unorm16);

	CHECK_FALSE(is_16bit_result_8bit);
	CHECK(is_16bit_result_16bit);
}
