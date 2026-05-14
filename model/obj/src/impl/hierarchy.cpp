#include "hierarchy.hpp"
#include "common/number-literals.hpp"
#include "model/hierarchy.hpp"

#include <cstdint>
#include <libassert/assert.hpp>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace model::obj::impl
{
	Hierarchy create_hierarchy(uint32_t mesh_count) noexcept
	{
		std::vector<ParentOnlyNode> nodes;

		// Parent Node
		nodes.push_back(ParentOnlyNode{.parent_index = std::nullopt, .data = {}});

		// Child nodes
		nodes.append_range(
			std::views::iota(0_u32, mesh_count) | std::views::transform([](uint32_t mesh_index) {
				return ParentOnlyNode{.parent_index = 0, .data = {.mesh_index = mesh_index}};
			})
		);

		auto result = Hierarchy::create(nodes);
		ASSERT(result.has_value());  // Should succeed

		return std::move(*result);
	}
}
