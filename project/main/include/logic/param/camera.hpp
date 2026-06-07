#pragma once

#include "render/interface/camera.hpp"
#include "scene/camera.hpp"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_uint2_sized.hpp>
#include <optional>

namespace logic
{
	///
	/// @brief Camera parameters
	///
	struct Camera
	{
		/*===== Parameters =====*/

		scene::camera::CenterView target_view = {
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

		/*===== States =====*/

		std::optional<glm::mat4> prev_view_proj_matrix = std::nullopt;
		std::optional<glm::vec3> prev_camera_pos = std::nullopt;

		std::optional<scene::camera::CenterView> curr_view = std::nullopt;
		double smooth_factor = 10;

		/*===== Functions =====*/

		///
		/// @brief Update the parameters based in mouse/keyboard input
		///
		/// @param extent Swapchain extent
		///
		void update_view(glm::u32vec2 extent) noexcept;

		///
		/// @brief Configuration UI
		///
		void config_ui() noexcept;

		///
		/// @brief Get camera parameters and update internal state
		///
		/// @param extent Swapchain extent
		/// @return Camera parameters
		///
		[[nodiscard]]
		render::Camera get_and_update(glm::u32vec2 extent) noexcept;
	};
}
