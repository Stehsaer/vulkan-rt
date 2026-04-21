#include "model/material.hpp"
#include "image/image.hpp"

namespace model
{

	image::Image<image::Format::Unorm8, image::Layout::RGBA>
	TextureSet::get_general_fallback_texture() noexcept
	{
		return {
			{4, 4},
			{255, 255, 255, 255}
		};
	}

	image::Image<image::Format::Unorm8, image::Layout::RGBA>
	TextureSet::get_normal_map_fallback_texture() noexcept
	{
		return {
			{4, 4},
			{128, 128, 255, 255}
		};
	}

	std::expected<MaterialList, Error> MaterialList::create(
		std::vector<Texture> textures,
		std::vector<Material> materials
	) noexcept
	{
		constexpr auto checks = std::to_array<
			std::tuple<std::string_view, std::optional<uint32_t> TextureSet::*, bool TextureUsage::*>
		>({
			{"base color",         &TextureSet::albedo,             &TextureUsage::srgb  },
			{"emissive",           &TextureSet::emissive,           &TextureUsage::srgb  },
			{"normal",             &TextureSet::normal,             &TextureUsage::normal},
			{"roughness-metallic", &TextureSet::roughness_metallic, &TextureUsage::linear}
		});

		std::vector<TextureUsage> texture_usages(textures.size());

		for (const auto& [index, material] : std::views::enumerate(materials))
		{
			for (const auto& [texture_name, texture_ref_member, usage_member] : checks)
			{
				const auto& texture_ref = material.texture_set.*texture_ref_member;
				if (!texture_ref.has_value()) continue;
				if (*texture_ref >= textures.size())
				{
					return Error(
						"Invalid material with out-of-bound texture reference",
						std::format(
							"Material #{} references invalid {} texture index #{} (total {})",
							index,
							texture_name,
							*texture_ref,
							textures.size()
						)
					);
				}

				texture_usages[*texture_ref].*usage_member = true;
			}
		}

		auto texture_pair =
			std::views::zip_transform(
				[](Texture& texture, TextureUsage usage) { return std::pair(std::move(texture), usage); },
				textures,
				texture_usages
			)
			| std::ranges::to<std::vector>();

		return MaterialList(std::move(texture_pair), std::move(materials));
	}
}
