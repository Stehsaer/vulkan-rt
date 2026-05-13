#pragma once

#include "param/auto-exposure.hpp"
#include "param/camera.hpp"
#include "param/primary-light.hpp"

#include <glm/ext/vector_uint2_sized.hpp>

namespace logic
{
	///
	/// @brief Parameters (Logic Layer)
	///
	struct Param
	{
		Camera camera;
		PrimaryLight primary_light;
		Exposure exposure;

		///
		/// @brief UI configuration window
		///
		/// @param extent Swapchain extent
		///
		void ui(glm::u32vec2 extent) noexcept;
	};
}
