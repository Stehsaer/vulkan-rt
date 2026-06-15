#include "logic/param/camera.hpp"
#include "render/interface/camera.hpp"
#include "scene/camera.hpp"

#include <glm/common.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_uint2_sized.hpp>
#include <glm/matrix.hpp>
#include <imgui.h>
#include <libassert/assert.hpp>

namespace logic
{
	void Camera::update_view(glm::u32vec2 extent) noexcept
	{
		auto& io = ImGui::GetIO();

		if (!io.WantCaptureMouse)
		{
			const auto mouse_delta = glm::vec2(io.MouseDelta.x, io.MouseDelta.y) / glm::vec2(extent);
			const auto mouse_scroll = io.MouseWheel;

			if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
				target_view = target_view.mouse_rotate(mouse_delta);

			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
				target_view =
					target_view.mouse_pan(mouse_delta, extent.x / static_cast<double>(extent.y), 1.0);

			target_view = target_view.mouse_scroll(mouse_scroll);
		}

		curr_view = scene::camera::CenterView::mix(
			curr_view.value_or(target_view),
			target_view,
			glm::clamp(smooth_factor * io.DeltaTime, 0.0, 1.0)
		);
	}

	void Camera::config_ui() noexcept
	{
		float temp_near = projection.near, temp_fov = projection.fov_degrees;

		ImGui::SliderFloat("FOV", &temp_fov, 30.0f, 150.0f, "%.0f deg");
		ImGui::SliderFloat("Near Plane", &temp_near, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

		projection.fov_degrees = temp_fov;
		projection.near = temp_near;
	}

	render::Camera Camera::get_and_update(glm::u32vec2 extent) noexcept
	{
		DEBUG_ASSERT(curr_view.has_value());

		const auto aspect_ratio = static_cast<double>(extent.x) / static_cast<double>(extent.y);
		const auto view_matrix = curr_view->matrix();
		const auto proj_matrix = projection.matrix(aspect_ratio);

		const auto view_proj_matrix = scene::camera::reverse_z() * proj_matrix * view_matrix;
		const auto prev_view_proj_matrix = this->prev_view_proj_matrix.value_or(view_proj_matrix);
		this->prev_view_proj_matrix = view_proj_matrix;

		const auto camera_pos = curr_view->view_position();
		const auto prev_camera_pos = this->prev_camera_pos.value_or(camera_pos);
		this->prev_camera_pos = camera_pos;

		const auto inv_view_proj_matrix = glm::inverse(view_proj_matrix);
		const auto back_projection_matrix = prev_view_proj_matrix * inv_view_proj_matrix;

		return {
			.inv_view_projection = inv_view_proj_matrix,
			.prev_view_projection = prev_view_proj_matrix,
			.view_projection = view_proj_matrix,
			.back_projection = back_projection_matrix,
			.camera_pos = camera_pos,
			.prev_camera_pos = prev_camera_pos,
		};
	}
}
