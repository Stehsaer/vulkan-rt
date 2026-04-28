#pragma once

#include "scene/camera.hpp"

#include <glm/glm.hpp>

namespace render
{
	///
	/// @brief Camera parameter
	///
	struct Camera
	{
		glm::mat4 inv_view_projection;
		glm::mat4 prev_view_projection;
		glm::mat4 view_projection;
		glm::vec3 camera_pos;
		glm::vec3 prev_camera_pos;
	};
}
