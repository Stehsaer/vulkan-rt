#include "material.hpp"

namespace model::obj::impl
{
	std::optional<uint32_t> TextureCache::get_texture_id(
		const std::string& path,
		const std::function<TextureLoader>& texture_loader
	) noexcept
	{
		// Look up in cache first
		if (path_texture_cache.contains(path)) return path_texture_cache.at(path);

		// No texture loader
		if (texture_loader == nullptr) return std::nullopt;

		auto texture_data_result = texture_loader(path);

		// Texture doesn't exist
		if (std::holds_alternative<std::monostate>(texture_data_result)) return std::nullopt;

		const auto id = textures.size();

		// From bytes
		if (std::holds_alternative<std::vector<std::byte>>(texture_data_result))
		{
			textures.emplace_back(
				Texture{.source = std::get<std::vector<std::byte>>(texture_data_result), .flip_y = true}
			);
			path_texture_cache[path] = id;
		}
		// From path
		else
		{
			textures.emplace_back(
				Texture{.source = std::get<std::filesystem::path>(texture_data_result), .flip_y = true}
			);
			path_texture_cache[path] = id;
		}

		return id;
	}

	// Convert from material float[3] to `glm::vec3`
	template <typename T>
	static glm::vec3 from_tinyobj_float3(const T& val) noexcept
	{
		return {val[0], val[1], val[2]};
	}

	Material::Param get_material_param(const tinyobj::material_t& material) noexcept
	{
		return {
			.base_color_factor = glm::vec4(from_tinyobj_float3(material.diffuse), 1.0f),
			.metallic_factor = 0.0,
			.roughness_factor = 0.2,
			.normal_scale = glm::vec2(-1.0f, 1.0f)  // Flip X axis
		};
	}

	std::expected<MaterialList, Error> convert_materials(
		const std::function<TextureLoader>& texture_loader,
		std::span<const tinyobj::material_t> materials
	) noexcept
	{
		TextureCache texture_cache;

		// Albedo only for now, since obj file treats roughness and metalness textures separately
		auto parsed_materials =
			materials
			| std::views::transform([&texture_cache, &texture_loader](const tinyobj::material_t& material) {
				  const auto diffuse_material =
					  texture_cache.get_texture_id(material.diffuse_texname, texture_loader);
				  const auto normal_material =
					  texture_cache.get_texture_id(material.bump_texname, texture_loader);

				  const auto texture_set = TextureSet{.albedo = diffuse_material, .normal = normal_material};
				  const auto param = get_material_param(material);

				  return Material{
					  .param = param,
					  .mode = {.alpha_mode = model::AlphaMode::Mask, .double_sided = true},
					  .texture_set = texture_set,
				  };
			  })
			| std::ranges::to<std::vector>();

		return MaterialList::create(std::move(texture_cache.textures), std::move(parsed_materials));
	}
}
