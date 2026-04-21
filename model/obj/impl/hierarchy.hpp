#pragma once

#include "model/hierarchy.hpp"
#include <cstdint>

namespace model::obj::impl
{
	// Create a flat hierarchy
	[[nodiscard]]
	Hierarchy create_hierarchy(uint32_t mesh_count) noexcept;
}
