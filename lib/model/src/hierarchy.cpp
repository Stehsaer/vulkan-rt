#include "model/hierarchy.hpp"
#include "common/util/unpack.hpp"
#include <algorithm>
#include <functional>
#include <libassert/assert.hpp>
#include <queue>
#include <ranges>
#include <utility>

namespace model
{
	glm::mat4 Transform::to_matrix() const noexcept
	{
		const auto t = glm::translate(glm::mat4(1.0f), translation);
		const auto tr = t * glm::mat4(rotation);
		const auto trs = glm::scale(tr, scale);

		return trs;
	}

	static std::expected<std::vector<uint32_t>, Error> get_bfs(
		const std::vector<FullNode>& double_linked_nodes
	) noexcept
	{
		/* Check Root Nodes */

		auto root_node_candidates =
			std::views::iota(0zu, double_linked_nodes.size())
			| std::views::filter([&double_linked_nodes](size_t i) {
				  return !double_linked_nodes[i].parent_index.has_value();
			  })
			| std::ranges::to<std::vector>();

		switch (root_node_candidates.size())
		{
		case 0:  // No root nodes
			return Error("Cycle detected in hierarchy", "No root node found");
		case 1:
			break;
		default:  // Too many root nodes (>=2)
			return Error(
				"Multiple root nodes found in hierarchy",
				std::format("Root node indices: {}", root_node_candidates)
			);
		}

		const auto root_node = static_cast<uint32_t>(root_node_candidates[0]);

		/* BFS and Detect Cycle */

		std::vector<uint32_t> bfs_indices;
		std::queue<uint32_t> bfs_queue;
		std::vector<bool> visited(double_linked_nodes.size(), false);

		bfs_indices.reserve(double_linked_nodes.size());
		bfs_queue.push(root_node);

		while (!bfs_queue.empty())
		{
			const auto node = bfs_queue.front();
			bfs_queue.pop();

			bfs_indices.push_back(node);
			visited[node] = true;

			for (const auto child : double_linked_nodes[node].child_indices)
			{
				if (child >= double_linked_nodes.size()) [[unlikely]]
					return Error(
						"Invalid child index in hierarchy",
						std::format(
							"Child node #{} of node #{} is out of bound (total {})",
							child,
							node,
							double_linked_nodes.size()
						)
					);

				if (visited[child]) [[unlikely]]
					return Error(
						"Cycle detected in hierarchy",
						std::format("Child node #{} of node #{} is already visited", child, node)
					);
			}

			bfs_queue.push_range(double_linked_nodes[node].child_indices);
		}

		if (!std::ranges::all_of(visited, std::identity()))
			return Error(
				"Disconnected node detected in hierarchy",
				std::format(
					"Nodes: {}",
					std::views::iota(0zu, visited.size()) | std::views::filter([&visited](size_t i) {
						return !visited[i];
					})
				)
			);

		return bfs_indices;
	}

	std::vector<Hierarchy::Drawcall> Hierarchy::find_renderable_nodes(
		const std::vector<FullNode>& double_linked_nodes
	) noexcept
	{
		return double_linked_nodes
			| std::views::enumerate
			| std::views::filter([](size_t, const FullNode& node) {
				   return node.data.mesh_index.has_value();
			   } | util::tuple_args)
			| std::views::transform([](size_t idx, const FullNode& node) {
				   return Drawcall{
					   .node_index = static_cast<uint32_t>(idx),
					   .mesh_index = node.data.mesh_index.value()
				   };
			   } | util::tuple_args)
			| std::ranges::to<std::vector>();
	}

	template <>
	std::expected<Hierarchy, Error> Hierarchy::create(std::span<const ParentOnlyNode> nodes) noexcept
	{
		auto double_linked_nodes =
			nodes
			| std::views::transform([](const ParentOnlyNode& node) {
				  return FullNode{.parent_index = node.parent_index, .child_indices = {}, .data = node.data};
			  })
			| std::ranges::to<std::vector>();

		if (double_linked_nodes.empty()) return Error("Invalid hierarchy with no nodes");

		/* Build child indices */

		for (const auto [index, node] : double_linked_nodes | std::views::enumerate)
		{
			if (!node.parent_index.has_value()) [[unlikely]]
				continue;

			const auto parent_index = node.parent_index.value();
			if (std::cmp_equal(parent_index, index)) [[unlikely]]
				return Error(
					"Cycle detected in hierarchy",
					std::format("Node #{} can't be its own child", index)
				);

			if (parent_index >= double_linked_nodes.size()) [[unlikely]]
				return Error(
					"Invalid parent index in hierarchy",
					std::format(
						"Parent node #{} of node #{} is out of bound (total {})",
						parent_index,
						index,
						double_linked_nodes.size()
					)
				);

			double_linked_nodes[parent_index].child_indices.push_back(static_cast<uint32_t>(index));
		}

		auto bfs_result = get_bfs(double_linked_nodes);
		if (!bfs_result) return bfs_result.error();
		auto bfs_indices = std::move(*bfs_result);
		DEBUG_ASSERT(bfs_indices.size() == double_linked_nodes.size());

		auto renderable_nodes = find_renderable_nodes(double_linked_nodes);

		return Hierarchy(std::move(double_linked_nodes), std::move(bfs_indices), std::move(renderable_nodes));
	}

	template <>
	std::expected<Hierarchy, Error> Hierarchy::create(std::span<const ChildOnlyNode> nodes) noexcept
	{
		auto double_linked_nodes =
			nodes
			| std::views::transform([](const ChildOnlyNode& node) {
				  return FullNode{
					  .parent_index = std::nullopt,
					  .child_indices = node.child_indices,
					  .data = node.data
				  };
			  })
			| std::ranges::to<std::vector>();

		if (double_linked_nodes.empty()) return Error("Invalid hierarchy with no nodes");

		/* Generate Parents */

		for (const auto [index, node] : double_linked_nodes | std::views::enumerate)
		{
			// Remove repeat and sort
			const auto [first, last] = std::ranges::unique(node.child_indices);
			node.child_indices.erase(first, last);
			std::ranges::sort(node.child_indices);

			// Assign current node to each of its children
			for (const auto child_index : node.child_indices)
			{
				if (child_index >= double_linked_nodes.size()) [[unlikely]]
					return Error(
						"Invalid child index in hierarchy",
						std::format(
							"Child node #{} of node #{} is out of bound (total {})",
							child_index,
							index,
							double_linked_nodes.size()
						)
					);

				if (std::cmp_equal(child_index, index)) [[unlikely]]
					return Error(
						"Cycle detected in hierarchy",
						std::format("Node #{} can't be its own child", index)
					);

				auto& child_node = double_linked_nodes[child_index];

				if (child_node.parent_index.has_value()) [[unlikely]]
					return Error(
						"Multiple parent nodes found in hierarchy",
						std::format(
							"Node #{} has multiple parents: #{} and #{}",
							child_index,
							child_node.parent_index.value(),
							index
						)
					);

				child_node.parent_index = index;
			}
		}

		auto bfs_result = get_bfs(double_linked_nodes);
		if (!bfs_result) return bfs_result.error();
		auto bfs_indices = std::move(*bfs_result);

		DEBUG_ASSERT(bfs_indices.size() == double_linked_nodes.size());

		auto renderable_nodes = find_renderable_nodes(double_linked_nodes);

		return Hierarchy(std::move(double_linked_nodes), std::move(bfs_indices), std::move(renderable_nodes));
	}

	std::pmr::vector<glm::mat4> Hierarchy::compute_transforms(
		const glm::mat4& root_transform,
		std::pmr::memory_resource& memory_resource
	) const noexcept
	{
		std::pmr::vector<glm::mat4> transforms(nodes.size(), &memory_resource);

		for (const auto idx : bfs_indices)
		{
			const auto& node = nodes[idx];
			const auto curr_transform = node.data.transform.to_matrix();
			const auto parent_transform =
				node.parent_index.transform([&transforms](uint32_t parent) { return transforms[parent]; })
					.value_or(root_transform);

			transforms[idx] = parent_transform * curr_transform;
		}

		return transforms;
	}

	std::pmr::vector<Hierarchy::Drawcall> Hierarchy::get_drawcalls(
		std::pmr::memory_resource& memory_resource
	) const noexcept
	{
		std::pmr::vector<Drawcall> result(&memory_resource);
		result.assign_range(renderable_nodes);
		return result;
	}
}
