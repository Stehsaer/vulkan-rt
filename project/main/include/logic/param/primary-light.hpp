#pragma once

#include "render/interface/direct-light.hpp"

#include <glm/ext/scalar_constants.hpp>
#include <glm/glm.hpp>

namespace logic
{
	///
	/// @brief Primary light parameters
	///
	///
	struct PrimaryLight
	{
		float light_yaw_deg = 0.0f, light_pitch_deg = 45.0f;
		glm::vec3 light_color = glm::vec3(1.0f);
		float light_intensity = glm::pi<float>();

		///
		/// @brief Configuration UI
		///
		void config_ui() noexcept;

		///
		/// @brief Get direct light parameters
		///
		/// @return Direct light parameters
		///
		[[nodiscard]]
		render::DirectLight get() const noexcept;
	};
}
