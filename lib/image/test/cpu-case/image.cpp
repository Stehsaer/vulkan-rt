#include <doctest/doctest.h>
#include <print>
#include <span>

#include "common/test-macro.hpp"
#include "common/util/span.hpp"
#include "image/common.hpp"
#include "image/image.hpp"
#include "test-asset.hpp"

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

TEST_SUITE("Decode")
{
	TEST_CASE("8bit PNG")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		auto decoded_image_result = ImageType::decode(load8_png_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 2, 2);

		CHECK_VEC4_EQ((decoded_image[0, 0]), 255, 0, 0, 255);
		CHECK_VEC4_EQ((decoded_image[1, 0]), 0, 255, 0, 255);
		CHECK_VEC4_EQ((decoded_image[0, 1]), 0, 0, 255, 255);
		CHECK_VEC4_EQ((decoded_image[1, 1]), 255, 255, 255, 255);
	}

	TEST_CASE("16bit PNG")
	{
		using ImageType = image::Image<image::Format::Unorm16, image::Layout::RGBA>;

		auto decoded_image_result = ImageType::decode(load16_png_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 2, 2);

		CHECK_VEC4_EQ((decoded_image[0, 0]), 65535, 0, 0, 65535);
		CHECK_VEC4_EQ((decoded_image[1, 0]), 0, 65535, 0, 65535);
		CHECK_VEC4_EQ((decoded_image[0, 1]), 0, 0, 65535, 65535);
		CHECK_VEC4_EQ((decoded_image[1, 1]), 65535, 65535, 65535, 65535);
	}

	TEST_CASE("8bit JPG")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		auto decoded_image_result = ImageType::decode(load8_jpg_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 16, 16);

		CHECK_VEC4_EQ((decoded_image[0, 0]), 254, 0, 0, 255);
		CHECK_VEC4_EQ((decoded_image[8, 0]), 0, 255, 1, 255);
		CHECK_VEC4_EQ((decoded_image[0, 8]), 0, 0, 254, 255);
		CHECK_VEC4_EQ((decoded_image[8, 8]), 255, 255, 255, 255);
	}

	TEST_CASE("Large image PNG")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		auto decoded_image_result = ImageType::decode(checker_image_data);
		EXPECT_SUCCESS(decoded_image_result);

		const auto decoded_image = std::move(decoded_image_result.value());
		REQUIRE_VEC2_EQ(decoded_image.size, 256, 256);
	}

	TEST_CASE("Large image grey")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::Grey>;

		auto decoded_image_result = ImageType::decode(checker_image_data);
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

TEST_SUITE("Encode")
{
	static void test_encoding(std::span<const std::byte> data, image::EncodeFormat format)
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		auto decoded_image_result = ImageType::decode(data);
		EXPECT_SUCCESS(decoded_image_result);
		auto decoded_image = std::move(*decoded_image_result);

		auto roundtrip_image_result = decoded_image.encode(format).and_then(ImageType::decode);
		EXPECT_SUCCESS(roundtrip_image_result);
		auto roundtrip_image = std::move(*roundtrip_image_result);

		REQUIRE_VEC2_EQ_ALT(decoded_image.size, roundtrip_image.size);

		if (!std::ranges::all_of(
				std::views::zip_transform(
					[](const glm::u8vec4& x, const glm::u8vec4& y) { return glm::all(glm::equal(x, y)); },
					decoded_image.data,
					roundtrip_image.data
				),
				std::identity()
			))
		{
			FAIL("Image data mismatch");
		}
	}

	static void test_encoding_lossy(
		const std::string_view& name,
		std::span<const std::byte> data,
		image::EncodeFormat format
	)
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		auto decoded_image_result = ImageType::decode(data);
		EXPECT_SUCCESS(decoded_image_result);
		auto decoded_image = std::move(*decoded_image_result);

		auto roundtrip_image_result = decoded_image.encode(format).and_then(ImageType::decode);
		EXPECT_SUCCESS(roundtrip_image_result);
		auto roundtrip_image = std::move(*roundtrip_image_result);

		REQUIRE_VEC2_EQ_ALT(decoded_image.size, roundtrip_image.size);

		const auto square_error = std::views::zip_transform(
			[](const glm::u8vec4& x, const glm::u8vec4& y) {
				const auto diff = glm::f32vec4(x) - glm::f32vec4(y);
				const auto diff2 = diff * diff;
				return (diff2.x + diff2.y + diff2.z + diff2.w) / 4.0f;
			},
			decoded_image.data,
			roundtrip_image.data
		);
		const auto mse = std::ranges::fold_left_first(square_error, std::plus()).value()
			/ (decoded_image.size.x * decoded_image.size.y);

		std::println("(Lossy Encoding, {}) MSE = {:.2f}", name, mse);

		if (mse > 10) FAIL("MSE too large");
	}

	TEST_CASE("PNG")
	{
		test_encoding(checker_image_data, image::encode_format::Png());
	}

	TEST_CASE("PNG Complex")
	{
		test_encoding(complex_image_data, image::encode_format::Png());
	}

	TEST_CASE("JPG")
	{
		test_encoding_lossy("JPG Checker", checker_image_data, image::encode_format::Jpg());
	}

	TEST_CASE("JPG Complex")
	{
		test_encoding_lossy("JPG Complex", complex_image_data, image::encode_format::Jpg());
	}

	TEST_CASE("BMP")
	{
		test_encoding(checker_image_data, image::encode_format::Bmp());
	}

	TEST_CASE("BMP Complex")
	{
		test_encoding(complex_image_data, image::encode_format::Bmp());
	}
}

TEST_SUITE("Resize")
{
	TEST_CASE("Normal")
	{
		using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

		auto decoded_image_result = ImageType::decode(load8_png_data);
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

		auto decoded_image_result = ImageType::decode(checker_image_data);
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

			// min_log2_size too large, resize and return 1 level
			const auto result3 = img.generate_mipmap(10);
			REQUIRE_EQ(result3.size(), 1);
			CHECK_VEC2_EQ(result3[0].size, 1024, 1024);
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
		const auto img = image::Image<image::Format::Unorm8, image::Layout::RGBA>(glm::u32vec2(511, 512));

		const auto result = img.resize_and_generate_mipmap(0);
		REQUIRE_EQ(result.size(), 10);
		CHECK_VEC2_EQ(result[0].size, 512, 512);
		CHECK_VEC2_EQ(result[9].size, 1, 1);

		// min_log2_size too large, resize and return 1 level
		const auto result2 = img.resize_and_generate_mipmap(10);
		REQUIRE_EQ(result2.size(), 1);
		CHECK_VEC2_EQ(result2[0].size, 1024, 1024);

		// min_log2_size matches resized size (but not the original one), return resized image without mipmap
		const auto result3 = img.resize_and_generate_mipmap(9);
		REQUIRE_EQ(result3.size(), 1);
		CHECK_VEC2_EQ(result3[0].size, 512, 512);
	}
}

TEST_CASE("Is 16 bit")
{
	const auto is_16bit_result_8bit = image::encoded_data_is_16bit(load8_png_data);
	const auto is_16bit_result_16bit = image::encoded_data_is_16bit(load16_png_data);

	CHECK_FALSE(is_16bit_result_8bit);
	CHECK(is_16bit_result_16bit);
}

TEST_CASE("Flip X")
{
	auto img_result = image::Image<image::Format::Unorm8, image::Layout::RGBA>::decode(complex_image_data);
	EXPECT_SUCCESS(img_result);
	auto img = std::move(*img_result);
	auto flipped_img = img.flip_x();

	for (const auto y : std::views::iota(0u, img.size.y))
		for (const auto x : std::views::iota(0u, img.size.x))
			if (img[x, y] != flipped_img[img.size.x - 1 - x, y]) FAIL("Pixel mismatched");
}

TEST_CASE("Flip Y")
{
	auto img_result = image::Image<image::Format::Unorm8, image::Layout::RGBA>::decode(complex_image_data);
	EXPECT_SUCCESS(img_result);
	auto img = std::move(*img_result);
	auto flipped_img = img.flip_y();

	for (const auto y : std::views::iota(0u, img.size.y))
		for (const auto x : std::views::iota(0u, img.size.x))
			if (img[x, y] != flipped_img[x, img.size.y - 1 - y]) FAIL("Pixel mismatched");
}
