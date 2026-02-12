#include "scene/camera.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene::camera
{
	glm::dmat4 LookatView::matrix() const noexcept
	{
		return glm::lookAt(position, look_position, up_direction);
	}

	glm::dmat4 CenterView::matrix() const noexcept
	{
		const auto pitch_radians = glm::radians(pitch_degrees);
		const auto yaw_radians = glm::radians(yaw_degrees);

		const auto direction = glm::dvec3{
			std::cos(pitch_radians) * std::sin(yaw_radians),
			std::sin(pitch_radians),
			std::cos(pitch_radians) * std::cos(yaw_radians)
		};

		const auto position = center_position + direction * distance;
		const auto up_direction = glm::dvec3(0.0, 1.0, 0.0);

		return glm::lookAt(position, center_position, up_direction);
	}

	CenterView CenterView::mouse_rotate(glm::dvec2 delta_uv, glm::dvec2 rotation_rate) const noexcept
	{
		const auto delta_angle = delta_uv * rotation_rate;

		const auto unclamped_pitch_degrees = pitch_degrees + delta_angle.y;
		const auto unclamped_yaw_degrees = yaw_degrees - delta_angle.x;

		const auto clamped_pitch_degrees = glm::clamp(unclamped_pitch_degrees, -89.9, 89.9);
		const auto clamped_yaw_degrees = std::fmod(unclamped_yaw_degrees, 360.0);

		return CenterView{
			.center_position = center_position,
			.distance = distance,
			.pitch_degrees = clamped_pitch_degrees,
			.yaw_degrees = clamped_yaw_degrees
		};
	}

	CenterView CenterView::mouse_scroll(double delta_scroll, double scroll_rate) const noexcept
	{
		const auto distance_mult = glm::pow(2.0, -delta_scroll * scroll_rate);
		const auto unclamped_distance = distance * distance_mult;
		const auto clamped_distance = glm::clamp(unclamped_distance, 1e-16, 1e16);

		return CenterView{
			.center_position = center_position,
			.distance = clamped_distance,
			.pitch_degrees = pitch_degrees,
			.yaw_degrees = yaw_degrees
		};
	}

	CenterView CenterView::mouse_pan(glm::dvec2 delta_uv, double aspect_ratio, double pan_rate) const noexcept
	{
		const auto calibrated_uv = glm::dvec2(delta_uv.x * aspect_ratio, delta_uv.y);

		const auto pitch_radians = glm::radians(pitch_degrees);
		const auto yaw_radians = glm::radians(yaw_degrees);

		const auto front_dir = glm::dvec3(
			std::cos(pitch_radians) * std::sin(yaw_radians),
			std::sin(pitch_radians),
			std::cos(pitch_radians) * std::cos(yaw_radians)
		);
		const auto right_dir = glm::normalize(glm::cross(front_dir, glm::dvec3(0.0, 1.0, 0.0)));
		const auto up_dir = glm::normalize(glm::cross(right_dir, front_dir));

		const auto pan_offset =
			(right_dir * calibrated_uv.x + up_dir * calibrated_uv.y) * pan_rate * distance;

		return CenterView{
			.center_position = center_position + pan_offset,
			.distance = distance,
			.pitch_degrees = pitch_degrees,
			.yaw_degrees = yaw_degrees
		};
	}

	CenterView CenterView::mix(const CenterView& x, const CenterView& y, double a) noexcept
	{
		double wrapped_yaw_x = std::fmod(x.yaw_degrees, 360.0);
		double wrapped_yaw_y = std::fmod(y.yaw_degrees, 360.0);

		if (std::abs(wrapped_yaw_x - wrapped_yaw_y) > 180.0)
		{
			if (wrapped_yaw_x > wrapped_yaw_y)
				wrapped_yaw_y += 360.0;
			else
				wrapped_yaw_x += 360.0;
		}

		const auto final_yaw = std::fmod(glm::mix(wrapped_yaw_x, wrapped_yaw_y, a), 360.0);

		return CenterView{
			.center_position = glm::mix(x.center_position, y.center_position, a),
			.distance = glm::mix(x.distance, y.distance, a),
			.pitch_degrees = glm::mix(x.pitch_degrees, y.pitch_degrees, a),
			.yaw_degrees = final_yaw
		};
	}

	glm::dmat4 PerspectiveProjection::matrix(double aspect_ratio) const noexcept
	{
		if (far.has_value())
			return glm::perspective(glm::radians(fov_degrees), aspect_ratio, near, far.value());
		else
			return glm::infinitePerspective(glm::radians(fov_degrees), aspect_ratio, near);
	}

	glm::dmat4 reverse_z(bool reverse) noexcept
	{
		if (reverse)
		{
			constexpr glm::dvec4 col0 = {1.0, 0.0, 0.0, 0.0};
			constexpr glm::dvec4 col1 = {0.0, 1.0, 0.0, 0.0};
			constexpr glm::dvec4 col2 = {0.0, 0.0, -1.0, 0.0};
			constexpr glm::dvec4 col3 = {0.0, 0.0, 1.0, 1.0};

			return {col0, col1, col2, col3};
		}
		else
		{
			return {1.0};
		}
	}
}