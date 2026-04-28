#pragma once

#include "render/interface/camera.hpp"
#include "scene/camera.hpp"

namespace logic
{
	///
	/// @brief Camera parameters
	///
	struct Camera
	{
		scene::camera::CenterView view = {
			.center_position = {0.0, 0.0, 0.0},
			.distance = 3.0,
			.pitch_degrees = 30.0,
			.yaw_degrees = 45.0,
		};

		scene::camera::PerspectiveProjection projection = {
			.fov_degrees = 50.0,
			.near = 0.01,
			.far = std::nullopt,  // Defaults to infinite-z
		};

		///
		/// @brief Update the parameters based in mouse/keyboard input
		///
		/// @param extent Swapchain extent
		///
		void update(glm::u32vec2 extent) noexcept;

		///
		/// @brief Configuration UI
		///
		void config_ui() noexcept;

		///
		/// @brief Get camera parameters
		///
		/// @param extent Swapchain extent
		/// @return Camera parameters
		///
		[[nodiscard]]
		render::Camera get(glm::u32vec2 extent) const noexcept;
	};
}
