#include "model/texture.hpp"
#include "common/file.hpp"
#include "common/test-macro.hpp"

#include <doctest.h>

constexpr auto test_bmp = std::to_array<uint8_t>({
	0x42, 0x4d, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28,
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x23, 0x2e, 0x00, 0x00, 0x23, 0x2e, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00,
});

TEST_CASE("From File Path")
{
	const auto temp_dir = std::filesystem::temp_directory_path();
	const auto temp_file = temp_dir / "test_texture.bmp";

	EXPECT_SUCCESS(file::write(temp_file, util::as_bytes(test_bmp)));
	const auto texture = model::Texture{.source = temp_file};

	auto load_result = texture.load();
	EXPECT_SUCCESS(load_result);

	auto image_variant = std::move(*load_result);
	REQUIRE(std::holds_alternative<image::Image<image::Format::Unorm8, image::Layout::RGBA>>(image_variant));

	auto image_8bit = std::get<image::Image<image::Format::Unorm8, image::Layout::RGBA>>(image_variant);
	REQUIRE_VEC2_EQ(image_8bit.size, 1, 1);

	auto pixel = image_8bit.data[0];
	CHECK_VEC4_EQ(pixel, 255, 255, 255, 255);
}

TEST_CASE("From Encoded Data")
{
	const auto texture =
		model::Texture{.source = std::vector<std::byte>(std::from_range, util::as_bytes(test_bmp))};

	auto load_result = texture.load();
	EXPECT_SUCCESS(load_result);

	auto image_variant = std::move(*load_result);
	REQUIRE(std::holds_alternative<image::Image<image::Format::Unorm8, image::Layout::RGBA>>(image_variant));

	auto image_8bit = std::get<image::Image<image::Format::Unorm8, image::Layout::RGBA>>(image_variant);
	REQUIRE_VEC2_EQ(image_8bit.size, 1, 1);

	auto pixel = image_8bit.data[0];
	CHECK_VEC4_EQ(pixel, 255, 255, 255, 255);
}

TEST_CASE("From Raw RGBA8 Image")
{
	const image::Image<image::Format::Unorm8, image::Layout::RGBA> raw_image(
		{1, 1},
		{0x80, 0x80, 0x80, 0x80}
	);
	const auto texture = model::Texture{.source = raw_image};

	SUBCASE("Load")
	{
		auto load_result = texture.load();
		EXPECT_SUCCESS(load_result);

		auto image_variant = std::move(*load_result);
		REQUIRE(
			std::holds_alternative<image::Image<image::Format::Unorm8, image::Layout::RGBA>>(image_variant)
		);

		auto image_8bit = std::get<image::Image<image::Format::Unorm8, image::Layout::RGBA>>(image_variant);
		REQUIRE_VEC2_EQ(image_8bit.size, 1, 1);

		auto pixel = image_8bit.data[0];
		CHECK_VEC4_EQ(pixel, 0x80, 0x80, 0x80, 0x80);
	}

	SUBCASE("Load 8")
	{
		auto load_result = texture.load_8bit();
		EXPECT_SUCCESS(load_result);

		auto image_8bit = std::move(*load_result);
		REQUIRE_VEC2_EQ(image_8bit.size, 1, 1);

		auto pixel = image_8bit.data[0];
		CHECK_VEC4_EQ(pixel, 0x80, 0x80, 0x80, 0x80);
	}

	SUBCASE("Load 16")
	{
		auto load_result = texture.load_16bit();
		EXPECT_SUCCESS(load_result);

		auto image_16bit = std::move(*load_result);
		REQUIRE_VEC2_EQ(image_16bit.size, 1, 1);

		auto pixel = image_16bit.data[0];
		CHECK_VEC4_EQ(pixel, 0x8080, 0x8080, 0x8080, 0x8080);
	}
}

TEST_CASE("From Raw RGBA16 Image")
{
	const image::Image<image::Format::Unorm16, image::Layout::RGBA> raw_image(
		{1, 1},
		{0x1234, 0x1234, 0x1234, 0x1234}
	);
	const auto texture = model::Texture{.source = raw_image};

	auto load_result = texture.load();
	EXPECT_SUCCESS(load_result);

	SUBCASE("Load")
	{
		auto image_variant = std::move(*load_result);
		REQUIRE(
			std::holds_alternative<image::Image<image::Format::Unorm16, image::Layout::RGBA>>(image_variant)
		);

		auto image_16bit = std::get<image::Image<image::Format::Unorm16, image::Layout::RGBA>>(image_variant);
		REQUIRE_VEC2_EQ(image_16bit.size, 1, 1);

		auto pixel = image_16bit.data[0];
		CHECK_VEC4_EQ(pixel, 0x1234, 0x1234, 0x1234, 0x1234);
	}

	SUBCASE("Load 8")
	{
		auto load_result = texture.load_8bit();
		EXPECT_SUCCESS(load_result);

		auto image_8bit = std::move(*load_result);
		REQUIRE_VEC2_EQ(image_8bit.size, 1, 1)

		auto pixel = image_8bit.data[0];
		CHECK_VEC4_EQ(pixel, 0x12, 0x12, 0x12, 0x12);
	}

	SUBCASE("Load 16")
	{
		auto load_result = texture.load_16bit();
		EXPECT_SUCCESS(load_result);

		auto image_16bit = std::move(*load_result);
		REQUIRE_VEC2_EQ(image_16bit.size, 1, 1);

		auto pixel = image_16bit.data[0];
		CHECK_VEC4_EQ(pixel, 0x1234, 0x1234, 0x1234, 0x1234);
	}
}
