#include "model/model.hpp"

#include <ranges>

namespace model
{
	std::expected<Model, Error> Model::assemble(
		MaterialList material_list,
		std::vector<Mesh> meshes,
		Hierarchy hierarchy
	) noexcept
	{
		/* Verify Meshes */

		const auto material_count = material_list.materials.size();
		for (const auto [mesh_idx, mesh] : meshes | std::views::enumerate)
			for (const auto [primitive_idx, primitive] : mesh.primitives | std::views::enumerate)
			{
				if (!primitive.material_index.has_value()) continue;
				if (primitive.material_index.value() >= material_count)
					return Error(
						"Material index of a primitive is out of bound",
						std::format(
							"Primitive #{} of mesh #{} has a material index of #{} which goes out of "
							"bound (total {})",
							primitive_idx,
							mesh_idx,
							primitive.material_index.value(),
							material_count
						)
					);
			}

		/* Verify Hierarchy */

		const auto mesh_count = meshes.size();
		for (const auto& [node_idx, mesh_idx] : hierarchy.get_drawcalls())
			if (mesh_idx >= mesh_count)
				return Error(
					"Mesh index of a node is out of bound",
					std::format(
						"Node #{} has a mesh index of #{} which goes out of bound (total {})",
						node_idx,
						mesh_idx,
						mesh_count
					)
				);

		return Model(std::move(material_list), std::move(meshes), std::move(hierarchy));
	}
}
