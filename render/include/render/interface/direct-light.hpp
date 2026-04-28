#pragma once

#include <glm/glm.hpp>

namespace render
{
	///
	/// @brief Directional light parameters
	///
	struct DirectLight
	{
		glm::vec3 direction;   // Direction of the light
		glm::vec3 light;       // RGB intensity of the light
		float sin_half_angle;  // Sine of half the angle of the light cone
	};
}
