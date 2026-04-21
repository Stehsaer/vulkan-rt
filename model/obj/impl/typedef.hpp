#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <tiny_obj_loader.h>
#include <vector>

namespace model::obj::impl
{
	using TriangleIndices = std::array<tinyobj::index_t, 3>;
	using PrimitiveMaterialMap =
		std::vector<std::pair<std::optional<uint32_t>, std::vector<TriangleIndices>>>;
}
