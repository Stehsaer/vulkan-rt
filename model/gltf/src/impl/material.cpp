#include "material.hpp"
#include "fastgltf-vec.hpp"
#include <fastgltf/types.hpp>

namespace model::gltf::impl
{
	Material parse_material(const fastgltf::Material& material) noexcept
	{
		Material::Param param;
		param.base_color_factor = to_glm(material.pbrData.baseColorFactor);
		param.emissive_factor = to_glm(material.emissiveFactor);
		param.alpha_cutoff = material.alphaCutoff;
		param.metallic_factor = material.pbrData.metallicFactor;
		param.roughness_factor = material.pbrData.roughnessFactor;
		param.normal_scale = glm::vec2(
			material.normalTexture.transform([](const auto& info) { return info.scale; }).value_or(1.0f)
		);

		Material::Mode mode;
		mode.double_sided = material.doubleSided;
		switch (material.alphaMode)
		{
		case fastgltf::AlphaMode::Opaque:
			mode.alpha_mode = AlphaMode::Opaque;
			break;
		case fastgltf::AlphaMode::Mask:
			mode.alpha_mode = AlphaMode::Mask;
			break;
		case fastgltf::AlphaMode::Blend:
			mode.alpha_mode = AlphaMode::Blend;
			break;
		}

		const auto get_texture_index = [](const auto& info) {
			return info.textureIndex;
		};

		TextureSet texture_set;
		texture_set.albedo = material.pbrData.baseColorTexture.transform(get_texture_index);
		texture_set.emissive = material.emissiveTexture.transform(get_texture_index);
		texture_set.normal = material.normalTexture.transform(get_texture_index);
		texture_set.roughness_metallic =
			material.pbrData.metallicRoughnessTexture.transform(get_texture_index);

		return {
			.param = param,
			.mode = mode,
			.texture_set = texture_set,
		};
	}

	std::expected<MaterialList, Error> parse_materials(Asset& asset, std::vector<Texture> textures) noexcept
	{
		auto materials =
			asset.materials | std::views::transform(parse_material) | std::ranges::to<std::vector>();

		return MaterialList::create(std::move(textures), std::move(materials))
			.transform_error(Error::forward_func("Create material list failed"));
	}
}
