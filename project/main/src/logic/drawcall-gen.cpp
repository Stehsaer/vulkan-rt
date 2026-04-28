#include "logic/drawcall-gen.hpp"

namespace logic
{
	DrawcallGenerator::Result DrawcallGenerator::compute(
		const render::Model& model,
		glm::mat4 model_transform
	) noexcept
	{
		const auto renderable_nodes = model.hierarchy.get_drawcalls(*memory_resource);

		render::PerRenderState<std::pmr::vector<render::PrimitiveDrawcall>> drawcalls;
		for (const auto [node, mesh] : renderable_nodes)
		{
			const auto primitive_range = model.mesh_list->mesh_ranges_array[mesh];

			for (const auto primitive_idx :
				 std::views::iota(primitive_range.offset, primitive_range.offset + primitive_range.count))
			{
				const auto primitive_attr = model.mesh_list->primitive_attr_array[primitive_idx];
				const auto material_mode =
					model.material_list.query_material_mode(primitive_attr.material_index);

				drawcalls[material_mode].emplace_back(
					render::PrimitiveDrawcall{.node_index = node, .primitive_index = primitive_idx}
				);
			}
		}

		return {
			.drawcalls = std::move(drawcalls),
			.transforms = model.hierarchy.compute_transforms(model_transform, *memory_resource)
		};
	}
}
