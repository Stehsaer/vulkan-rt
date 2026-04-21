#pragma once

#include "model/obj.hpp"
#include "typedef.hpp"

#include <functional>
#include <map>
#include <tiny_obj_loader.h>

namespace model::obj::impl
{
	struct TextureCache
	{
		std::vector<Texture> textures;
		std::map<std::string, size_t> path_texture_cache;  // maps a path to a texture ID in `textures`

		[[nodiscard]]
		std::optional<uint32_t> get_texture_id(
			const std::string& path,
			const std::function<TextureLoader>& texture_loader
		) noexcept;
	};

	// Get material parameters from tinyobj material
	[[nodiscard]]
	Material::Param get_material_param(const tinyobj::material_t& material) noexcept;

	// Convert all tinyobj materials into `MaterialList`
	[[nodiscard]]
	std::expected<MaterialList, Error> convert_materials(
		const std::function<TextureLoader>& texture_loader,
		std::span<const tinyobj::material_t> materials
	) noexcept;
}
