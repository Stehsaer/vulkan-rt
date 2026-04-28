#include <doctest.h>

#include "common/test-macro.hpp"
#include "render/model/texture.hpp"
#include "vulkan/test-driver.hpp"
#include "vulkan/util/static-resource-creator.hpp"

// NOLINTBEGIN

static const auto texture_8bit = model::Texture{
	.source = image::Image<image::Format::Unorm8, image::Layout::RGBA>({16, 16}, {0x80, 0x80, 0x80, 0x80})
};

static const auto texture_16bit = model::Texture{
	.source =
		image::Image<image::Format::Unorm16, image::Layout::RGBA>({16, 16}, {0x8000, 0x8000, 0x8000, 0x8000})
};

static const auto texture_large = model::Texture{
	.source = image::Image<image::Format::Unorm8, image::Layout::RGBA>({2048, 2048}, {0x80, 0x80, 0x80, 0x80})
};

static const auto texture_large_npot = model::Texture{
	.source = image::Image<image::Format::Unorm8, image::Layout::RGBA>({1000, 1000}, {0x80, 0x80, 0x80, 0x80})
};

static constexpr auto SMALL_TEX_RAW_MIPMAP_LEVELS = 5;
static constexpr auto SMALL_TEX_BC_MIPMAP_LEVELS = SMALL_TEX_RAW_MIPMAP_LEVELS - 2;
static constexpr auto LARGE_TEX_RAW_MIPMAP_LEVELS = 12;
static constexpr auto LARGE_TEX_BC_MIPMAP_LEVELS = LARGE_TEX_RAW_MIPMAP_LEVELS - 2;
static constexpr auto LARGE_TEX_NPOT_RAW_MIPMAP_LEVELS = 11;
static constexpr auto LARGE_TEX_NPOT_BC_MIPMAP_LEVELS = LARGE_TEX_NPOT_RAW_MIPMAP_LEVELS - 2;

// NOLINTEND

TEST_CASE("Raw")
{
	vulkan::StaticResourceCreator resource_creator(vulkan::get_test_context().get());

	auto load_result_8bit = render::Texture::load_color_texture(
		resource_creator,
		texture_8bit,
		render::Texture::ColorLoadStrategy::Raw
	);
	EXPECT_SUCCESS(load_result_8bit);
	auto loaded_texture_8bit = std::move(*load_result_8bit);
	CHECK_EQ(loaded_texture_8bit.format, render::Texture::Format::Rgba8Unorm);
	CHECK_EQ(loaded_texture_8bit.mipmap_levels, SMALL_TEX_RAW_MIPMAP_LEVELS);

	auto load_result_16bit = render::Texture::load_color_texture(
		resource_creator,
		texture_16bit,
		render::Texture::ColorLoadStrategy::Raw
	);
	EXPECT_SUCCESS(load_result_16bit);
	auto loaded_texture_16bit = std::move(*load_result_16bit);
	CHECK_EQ(loaded_texture_16bit.format, render::Texture::Format::Rgba8Unorm);
	CHECK_EQ(loaded_texture_16bit.mipmap_levels, SMALL_TEX_RAW_MIPMAP_LEVELS);

	auto load_result_large = render::Texture::load_color_texture(
		resource_creator,
		texture_large,
		render::Texture::ColorLoadStrategy::Raw
	);
	EXPECT_SUCCESS(load_result_large);
	auto loaded_texture_large = std::move(*load_result_large);
	CHECK_EQ(loaded_texture_large.format, render::Texture::Format::Rgba8Unorm);
	CHECK_EQ(loaded_texture_large.mipmap_levels, LARGE_TEX_RAW_MIPMAP_LEVELS);

	auto load_result_large_npot = render::Texture::load_color_texture(
		resource_creator,
		texture_large_npot,
		render::Texture::ColorLoadStrategy::Raw
	);
	EXPECT_SUCCESS(load_result_large_npot);
	auto loaded_texture_large_npot = std::move(*load_result_large_npot);
	CHECK_EQ(loaded_texture_large_npot.format, render::Texture::Format::Rgba8Unorm);
	CHECK_EQ(loaded_texture_large_npot.mipmap_levels, LARGE_TEX_NPOT_RAW_MIPMAP_LEVELS);

	const auto upload_result = resource_creator.execute_uploads();
	EXPECT_SUCCESS(upload_result);
}

TEST_CASE("All BC3")
{
	vulkan::StaticResourceCreator resource_creator(vulkan::get_test_context().get());

	auto load_result_8bit_bc3 = render::Texture::load_color_texture(
		resource_creator,
		texture_8bit,
		render::Texture::ColorLoadStrategy::AllBC3
	);
	EXPECT_SUCCESS(load_result_8bit_bc3);
	auto loaded_texture_8bit_bc3 = std::move(*load_result_8bit_bc3);
	CHECK_EQ(loaded_texture_8bit_bc3.format, render::Texture::Format::BC3);
	CHECK_EQ(loaded_texture_8bit_bc3.mipmap_levels, SMALL_TEX_BC_MIPMAP_LEVELS);

	auto load_result_16bit_bc3 = render::Texture::load_color_texture(
		resource_creator,
		texture_16bit,
		render::Texture::ColorLoadStrategy::AllBC3
	);
	EXPECT_SUCCESS(load_result_16bit_bc3);
	auto loaded_texture_16bit_bc3 = std::move(*load_result_16bit_bc3);
	CHECK_EQ(loaded_texture_16bit_bc3.format, render::Texture::Format::BC3);
	CHECK_EQ(loaded_texture_16bit_bc3.mipmap_levels, SMALL_TEX_BC_MIPMAP_LEVELS);

	auto load_result_large_bc3 = render::Texture::load_color_texture(
		resource_creator,
		texture_large,
		render::Texture::ColorLoadStrategy::AllBC3
	);
	EXPECT_SUCCESS(load_result_large_bc3);
	auto loaded_texture_large_bc3 = std::move(*load_result_large_bc3);
	CHECK_EQ(loaded_texture_large_bc3.format, render::Texture::Format::BC3);
	CHECK_EQ(loaded_texture_large_bc3.mipmap_levels, LARGE_TEX_BC_MIPMAP_LEVELS);

	auto load_result_large_npot_bc3 = render::Texture::load_color_texture(
		resource_creator,
		texture_large_npot,
		render::Texture::ColorLoadStrategy::AllBC3
	);
	EXPECT_SUCCESS(load_result_large_npot_bc3);
	auto loaded_texture_large_npot_bc3 = std::move(*load_result_large_npot_bc3);
	CHECK_EQ(loaded_texture_large_npot_bc3.format, render::Texture::Format::BC3);
	CHECK_EQ(loaded_texture_large_npot_bc3.mipmap_levels, LARGE_TEX_NPOT_BC_MIPMAP_LEVELS);

	const auto upload_result = resource_creator.execute_uploads();
	EXPECT_SUCCESS(upload_result);
}

TEST_CASE("All BC7")
{
	vulkan::StaticResourceCreator resource_creator(vulkan::get_test_context().get());

	auto load_result_8bit_bc7 = render::Texture::load_color_texture(
		resource_creator,
		texture_8bit,
		render::Texture::ColorLoadStrategy::AllBC7
	);
	EXPECT_SUCCESS(load_result_8bit_bc7);
	auto loaded_texture_8bit_bc7 = std::move(*load_result_8bit_bc7);
	CHECK_EQ(loaded_texture_8bit_bc7.format, render::Texture::Format::BC7);
	CHECK_EQ(loaded_texture_8bit_bc7.mipmap_levels, SMALL_TEX_BC_MIPMAP_LEVELS);

	auto load_result_16bit_bc7 = render::Texture::load_color_texture(
		resource_creator,
		texture_16bit,
		render::Texture::ColorLoadStrategy::AllBC7
	);
	EXPECT_SUCCESS(load_result_16bit_bc7);
	auto loaded_texture_16bit_bc7 = std::move(*load_result_16bit_bc7);
	CHECK_EQ(loaded_texture_16bit_bc7.format, render::Texture::Format::BC7);
	CHECK_EQ(loaded_texture_16bit_bc7.mipmap_levels, SMALL_TEX_BC_MIPMAP_LEVELS);

	auto load_result_large_bc7 = render::Texture::load_color_texture(
		resource_creator,
		texture_large,
		render::Texture::ColorLoadStrategy::AllBC7
	);
	EXPECT_SUCCESS(load_result_large_bc7);
	auto loaded_texture_large_bc7 = std::move(*load_result_large_bc7);
	CHECK_EQ(loaded_texture_large_bc7.format, render::Texture::Format::BC7);
	CHECK_EQ(loaded_texture_large_bc7.mipmap_levels, LARGE_TEX_BC_MIPMAP_LEVELS);

	auto load_result_large_npot_bc7 = render::Texture::load_color_texture(
		resource_creator,
		texture_large_npot,
		render::Texture::ColorLoadStrategy::AllBC7
	);
	EXPECT_SUCCESS(load_result_large_npot_bc7);
	auto loaded_texture_large_npot_bc7 = std::move(*load_result_large_npot_bc7);
	CHECK_EQ(loaded_texture_large_npot_bc7.format, render::Texture::Format::BC7);
	CHECK_EQ(loaded_texture_large_npot_bc7.mipmap_levels, LARGE_TEX_NPOT_BC_MIPMAP_LEVELS);

	const auto upload_result_bc7 = resource_creator.execute_uploads();
	EXPECT_SUCCESS(upload_result_bc7);
}

TEST_CASE("BalancedBC")
{
	vulkan::StaticResourceCreator resource_creator(vulkan::get_test_context().get());

	// Balanced: use BC7 for small textures, BC3 for > 1024px

	auto load_result_8bit_bal = render::Texture::load_color_texture(
		resource_creator,
		texture_8bit,
		render::Texture::ColorLoadStrategy::BalancedBC
	);
	EXPECT_SUCCESS(load_result_8bit_bal);
	auto loaded_texture_8bit_bal = std::move(*load_result_8bit_bal);
	CHECK_EQ(loaded_texture_8bit_bal.format, render::Texture::Format::BC7);
	CHECK_EQ(loaded_texture_8bit_bal.mipmap_levels, SMALL_TEX_BC_MIPMAP_LEVELS);

	auto load_result_16bit_bal = render::Texture::load_color_texture(
		resource_creator,
		texture_16bit,
		render::Texture::ColorLoadStrategy::BalancedBC
	);
	EXPECT_SUCCESS(load_result_16bit_bal);
	auto loaded_texture_16bit_bal = std::move(*load_result_16bit_bal);
	CHECK_EQ(loaded_texture_16bit_bal.format, render::Texture::Format::BC7);
	CHECK_EQ(loaded_texture_16bit_bal.mipmap_levels, SMALL_TEX_BC_MIPMAP_LEVELS);

	auto load_result_large_bal = render::Texture::load_color_texture(
		resource_creator,
		texture_large,
		render::Texture::ColorLoadStrategy::BalancedBC
	);
	EXPECT_SUCCESS(load_result_large_bal);
	auto loaded_texture_large_bal = std::move(*load_result_large_bal);
	CHECK_EQ(loaded_texture_large_bal.format, render::Texture::Format::BC3);
	CHECK_EQ(loaded_texture_large_bal.mipmap_levels, LARGE_TEX_BC_MIPMAP_LEVELS);

	auto load_result_large_npot_bal = render::Texture::load_color_texture(
		resource_creator,
		texture_large_npot,
		render::Texture::ColorLoadStrategy::BalancedBC
	);
	EXPECT_SUCCESS(load_result_large_npot_bal);
	auto loaded_texture_large_npot_bal = std::move(*load_result_large_npot_bal);
	CHECK_EQ(loaded_texture_large_npot_bal.format, render::Texture::Format::BC7);
	CHECK_EQ(loaded_texture_large_npot_bal.mipmap_levels, LARGE_TEX_NPOT_BC_MIPMAP_LEVELS);

	const auto upload_result_bal = resource_creator.execute_uploads();
	EXPECT_SUCCESS(upload_result_bal);
}
