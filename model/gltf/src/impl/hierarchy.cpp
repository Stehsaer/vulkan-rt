#include "hierarchy.hpp"
#include "common/util/overload.hpp"
#include "fastgltf-vec.hpp"
#include "model/hierarchy.hpp"

#include <glm/gtx/matrix_decompose.hpp>

namespace model::gltf::impl
{
	ChildOnlyNode create_child_only_node(const fastgltf::Node& node) noexcept
	{
		const auto transform_visitor = util::Overload(
			[](const fastgltf::TRS& trs) {
				const auto& [translation, rotation, scale] = trs;
				return Transform{
					.scale = to_glm(scale),
					.rotation = glm::quat(rotation.w(), rotation.x(), rotation.y(), rotation.z()),
					.translation = to_glm(translation),
				};
			},
			[](const fastgltf::math::fmat4x4& matrix) {
				Transform transform;
				glm::vec3 placeholder_skew;
				glm::vec4 placeholder_perspective;

				const glm::mat4 mat = {
					to_glm(matrix.col(0)),
					to_glm(matrix.col(1)),
					to_glm(matrix.col(2)),
					to_glm(matrix.col(3))
				};

				glm::decompose(
					mat,
					transform.scale,
					transform.rotation,
					transform.translation,
					placeholder_skew,
					placeholder_perspective
				);

				return transform;
			}
		);

		auto child_indices = std::vector<uint32_t>(std::from_range, node.children);

		const auto transform = std::visit(transform_visitor, node.transform);
		const auto mesh_index = node.meshIndex.transform([](auto idx) { return uint32_t(idx); });
		const auto data = NodeData{.transform = transform, .mesh_index = mesh_index};

		return ChildOnlyNode{.child_indices = std::move(child_indices), .data = data};
	}

	std::expected<Hierarchy, Error> create_hierarchy(const fastgltf::Asset& asset) noexcept
	{
		if (asset.nodes.empty()) return Error("No nodes in the asset");
		if (asset.scenes.size() > 1) return Error("Multiple scenes are not supported");

		auto nodes =
			asset.nodes | std::views::transform(create_child_only_node) | std::ranges::to<std::vector>();

		// additional dummy root node to support multiple root nodes in glTF scene
		nodes.push_back({
			.child_indices = std::vector<uint32_t>(std::from_range, asset.scenes.front().nodeIndices),
			.data = NodeData(),
		});

		return Hierarchy::create(std::move(nodes));
	}
}
