#pragma once

#include "model/material.hpp"
#include "render/model/texture-list.hpp"

namespace test
{
	///
	/// @brief Create test material set with 64 textures and 16 materials
	///
	/// @return Created `MaterialList` with 64 textures and 16 materials
	///
	[[nodiscard]]
	model::MaterialList create_material_list();

	///
	/// @brief Create test material set with erroneous texture data
	///
	/// @return Created `MaterialList` with 64 textures containing erroneous data and 16 materials
	///
	[[nodiscard]]
	model::MaterialList create_erroneous_material_list();

	///
	/// @brief Test if the generated texture list is correct
	///
	/// @param texture_list Texture list to test
	///
	void check_texture_list(const render::TextureList& texture_list);
}
