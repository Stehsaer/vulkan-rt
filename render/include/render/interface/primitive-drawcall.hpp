#pragma once

#include <cstdint>

namespace render
{
	///
	/// @brief A drawcall for a primitive
	///
	struct PrimitiveDrawcall
	{
		uint32_t node_index;       // Index of the node, used for selecting the correct transform
		uint32_t primitive_index;  // Index of the primitive into global primitives
	};
}
