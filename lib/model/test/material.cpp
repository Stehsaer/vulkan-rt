#include "model/material.hpp"
#include "common/test-macro.hpp"

#include <doctest.h>

TEST_CASE("Valid Material Set")
{
	const auto texture = std::vector<model::Texture>(
		10,
		model::Texture{
			.source = image::Image<image::Format::Unorm8, image::Layout::RGBA>({1, 1}, {255, 255, 255, 255})
		}
	);

	const auto texture_set =
		model::TextureSet{.albedo = 0, .emissive = 1, .roughness_metallic = 2, .normal = 3};

	const auto material = model::Material{.texture_set = texture_set};

	auto material_list_result = model::MaterialList::create(texture, {material});
	EXPECT_SUCCESS(material_list_result);

	auto material_list = std::move(*material_list_result);
	REQUIRE(material_list.materials.size() == 1);
	REQUIRE(material_list.textures.size() == 10);

	CHECK(material_list.textures[0].second.srgb);
	CHECK_FALSE(material_list.textures[0].second.linear);
	CHECK_FALSE(material_list.textures[0].second.normal);

	CHECK(material_list.textures[1].second.srgb);
	CHECK_FALSE(material_list.textures[1].second.linear);
	CHECK_FALSE(material_list.textures[1].second.normal);

	CHECK(material_list.textures[2].second.linear);
	CHECK_FALSE(material_list.textures[2].second.srgb);
	CHECK_FALSE(material_list.textures[2].second.normal);

	CHECK(material_list.textures[3].second.normal);
	CHECK_FALSE(material_list.textures[3].second.srgb);
	CHECK_FALSE(material_list.textures[3].second.linear);

	for (const auto i : std::views::iota(4zu, 10zu))
	{
		CHECK_FALSE(material_list.textures[i].second.srgb);
		CHECK_FALSE(material_list.textures[i].second.linear);
		CHECK_FALSE(material_list.textures[i].second.normal);
	}
}

TEST_CASE("Out of bound Material Set")
{
	const auto texture = std::vector<model::Texture>(
		1,
		model::Texture{
			.source = image::Image<image::Format::Unorm8, image::Layout::RGBA>({1, 1}, {255, 255, 255, 255})
		}
	);

	const auto texture_set = model::TextureSet{
		.albedo = 0,
		.emissive = 1,  // Out of bound
	};

	const auto material = model::Material{.texture_set = texture_set};

	auto material_list_result = model::MaterialList::create(texture, {material});
	EXPECT_FAIL(material_list_result);
}
