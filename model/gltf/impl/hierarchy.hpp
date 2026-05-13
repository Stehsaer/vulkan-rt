#pragma once

#include "common/util/error.hpp"
#include "model/hierarchy.hpp"

#include <expected>
#include <fastgltf/types.hpp>

namespace model::gltf::impl
{
	[[nodiscard]]
	ChildOnlyNode create_child_only_node(const fastgltf::Node& node) noexcept;

	[[nodiscard]]
	std::expected<Hierarchy, Error> create_hierarchy(const fastgltf::Asset& asset) noexcept;
}
