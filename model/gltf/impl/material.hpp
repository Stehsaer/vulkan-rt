#pragma once

#include "asset.hpp"
#include "model/material.hpp"
#include "model/texture.hpp"
#include <expected>

namespace model::gltf::impl
{
	[[nodiscard]]
	Material parse_material(const fastgltf::Material& material) noexcept;

	[[nodiscard]]
	std::expected<MaterialList, Error> parse_materials(Asset& asset, std::vector<Texture> textures) noexcept;
}
