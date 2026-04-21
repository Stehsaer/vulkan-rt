#pragma once

#include "common/util/error.hpp"
#include "hierarchy.hpp"
#include "material.hpp"
#include "mesh.hpp"

namespace model
{
	///
	/// @brief Model class, represents a verified and immutable model, with all its index being valid
	/// @note Visit its members through `operator->`
	///
	class Model
	{
	  public:

		///
		/// @brief Material list of the model
		///
		MaterialList material_list;

		///
		/// @brief Mesh list of the model, stored in a `std::vector`
		///
		std::vector<Mesh> meshes;

		///
		/// @brief Hierarchy of the model
		///
		Hierarchy hierarchy;

		///
		/// @brief Create and verify a model from materials, meshes and hierarchy
		///
		/// @param material_list Input material set
		/// @param meshes Input meshes, stored in a `std::vector`
		/// @param hierarchy Input hierarchy
		/// @return Verified model, or `Error`
		///
		static std::expected<Model, Error> assemble(
			MaterialList material_list,
			std::vector<Mesh> meshes,
			Hierarchy hierarchy
		) noexcept;

	  private:

		explicit Model(MaterialList material_list, std::vector<Mesh> meshes, Hierarchy hierarchy) :
			material_list(std::move(material_list)),
			meshes(std::move(meshes)),
			hierarchy(std::move(hierarchy))
		{}

	  public:

		Model(const Model&) = delete;
		Model(Model&&) = default;
		Model& operator=(const Model&) = delete;
		Model& operator=(Model&&) = default;
	};
}
