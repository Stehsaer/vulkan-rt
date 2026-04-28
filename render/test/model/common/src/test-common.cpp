#include "test-common.hpp"
#include "common/test-macro.hpp"
#include "image/common.hpp"
#include "image/noise.hpp"

#include <doctest.h>

namespace test
{
	model::MaterialList create_material_list()
	{
		auto encoded_data_result =
			image::get_blue_noise()
				->map([](glm::u16vec4 pix) { return glm::u8vec4(pix >> uint16_t(8)); })
				.resize({512, 512}, STBIR_FILTER_BOX)
				.encode(image::encode_format::Png());
		EXPECT_SUCCESS(encoded_data_result);
		auto encoded_data = std::move(*encoded_data_result);

		auto textures = std::views::iota(0, 64)
			| std::views::transform([&encoded_data](auto) { return model::Texture{.source = encoded_data}; })
			| std::ranges::to<std::vector>();

		auto materials =
			std::views::iota(0, 16)
			| std::views::transform([](uint32_t i) {
				  const auto base_index = i * 4;
				  const auto texture_set = model::TextureSet{
					  .albedo = base_index,
					  .emissive = base_index,
					  .roughness_metallic = base_index + 1,
					  .normal = base_index + 2,
				  };

				  return model::Material{.texture_set = texture_set};
			  })
			| std::ranges::to<std::vector>();

		auto material_list_result = model::MaterialList::create(textures, materials);
		EXPECT_SUCCESS(material_list_result);
		return std::move(*material_list_result);
	}

	model::MaterialList create_erroneous_material_list()
	{
		auto encoded_data_result =
			image::get_blue_noise()
				->map([](glm::u16vec4 pix) { return glm::u8vec4(pix >> uint16_t(8)); })
				.encode(image::encode_format::Png());
		EXPECT_SUCCESS(encoded_data_result);
		auto encoded_data = std::move(*encoded_data_result);

		const auto error_data =
			std::vector<std::byte>{std::byte(0), std::byte(1), std::byte(2), std::byte(3)};

		auto textures =
			std::views::iota(0, 64)
			| std::views::transform([&encoded_data, &error_data](uint32_t idx) {
				  return model::Texture{.source = idx % 2 == 0 ? encoded_data : error_data};
			  })
			| std::ranges::to<std::vector>();

		auto materials =
			std::views::iota(0, 16)
			| std::views::transform([](uint32_t i) {
				  const auto base_index = i * 4;
				  const auto texture_set = model::TextureSet{
					  .albedo = base_index,
					  .emissive = base_index,
					  .roughness_metallic = base_index + 1,
					  .normal = base_index + 2,
				  };

				  return model::Material{.texture_set = texture_set};
			  })
			| std::ranges::to<std::vector>();

		auto material_list_result = model::MaterialList::create(textures, materials);
		EXPECT_SUCCESS(material_list_result);
		return std::move(*material_list_result);
	}

	void check_texture_list(const render::TextureList& texture_list)  // NOLINT
	{
		for (const auto idx : std::views::iota(0u, 16u))
		{
			const auto base_index = idx * 4;

			// Texture 0: color texture, no normal texture

			auto [texture0_color_texture, texture0_color_mode] = texture_list.get_color_texture(base_index);
			CHECK(texture0_color_texture.format == render::Texture::Format::BC7);
			CHECK_EQ(texture0_color_mode.max_mipmap_level, vk::LodClampNone);

			auto [texture0_normal_texture, texture0_normal_mode] =
				texture_list.get_normal_texture(base_index);
			CHECK_EQ(texture0_normal_mode.max_mipmap_level, 0);  // Error hint

			// Texture 1: color texture, no normal texture

			auto [texture1_color_texture, texture1_color_mode] =
				texture_list.get_color_texture(base_index + 1);
			CHECK(texture1_color_texture.format == render::Texture::Format::BC7);
			CHECK_EQ(texture1_color_mode.max_mipmap_level, vk::LodClampNone);

			auto [texture1_normal_texture, texture1_normal_mode] =
				texture_list.get_normal_texture(base_index + 1);
			CHECK_EQ(texture1_normal_mode.max_mipmap_level, 0);  // Error hint

			// Texture 2: normal texture, no color texture

			auto [texture2_color_texture, texture2_color_mode] =
				texture_list.get_color_texture(base_index + 2);
			CHECK_EQ(texture2_color_mode.max_mipmap_level, 0);  // Error hint

			auto [texture2_normal_texture, texture2_normal_mode] =
				texture_list.get_normal_texture(base_index + 2);
			CHECK(texture2_normal_texture.format == render::Texture::Format::BC5);
			CHECK_EQ(texture2_normal_mode.max_mipmap_level, vk::LodClampNone);

			// Texture 3: Unused

			auto [texture3_color_texture, texture3_color_mode] =
				texture_list.get_color_texture(base_index + 3);
			CHECK_EQ(texture3_color_mode.max_mipmap_level, 0);  // Error hint

			auto [texture3_normal_texture, texture3_normal_mode] =
				texture_list.get_normal_texture(base_index + 3);
			CHECK_EQ(texture3_normal_mode.max_mipmap_level, 0);  // Error hint
		}

		// Empty texture index, return fallback

		auto [empty_texture_color_texture, empty_texture_color_mode] =
			texture_list.get_color_texture(std::nullopt);
		CHECK(empty_texture_color_texture.format == render::Texture::Format::BC7);
		CHECK_EQ(empty_texture_color_mode.max_mipmap_level, vk::LodClampNone);

		auto [empty_texture_normal_texture, empty_texture_normal_mode] =
			texture_list.get_normal_texture(std::nullopt);
		CHECK(empty_texture_normal_texture.format == render::Texture::Format::BC5);
		CHECK_EQ(empty_texture_normal_mode.max_mipmap_level, vk::LodClampNone);

		// OOB texture index should return error hint texture

		auto [oob_texture_color_texture, oob_texture_color_mode] = texture_list.get_color_texture(64);
		CHECK_EQ(oob_texture_color_mode.max_mipmap_level, 0);  // Error hint

		auto [oob_texture_normal_texture, oob_texture_normal_mode] = texture_list.get_normal_texture(64);
		CHECK_EQ(oob_texture_normal_mode.max_mipmap_level, 0);  // Error hint
	}
}
