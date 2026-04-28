#pragma once

#include "primitive-drawcall.hpp"

#include <vulkan/vulkan.hpp>

namespace render
{
	///
	/// @brief A drawcall for an indirect draw, containing both the draw command and the primitive information
	/// @note `draw_command.firstInstance` is used for vertex shader to identify the drawcall
	///
	struct IndirectDrawcall
	{
		vk::DrawIndexedIndirectCommand draw_command;
		PrimitiveDrawcall primitive_drawcall;
	};
}
