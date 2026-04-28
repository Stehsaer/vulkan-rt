#include "logic/param/camera.hpp"

#include <imgui.h>

namespace logic
{
	void Camera::update(glm::u32vec2 extent) noexcept
	{
		auto& io = ImGui::GetIO();

		if (!io.WantCaptureMouse)
		{
			const auto mouse_delta = glm::vec2(io.MouseDelta.x, io.MouseDelta.y) / glm::vec2(extent);
			const auto mouse_scroll = io.MouseWheel;

			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) view = view.mouse_rotate(mouse_delta);

			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
				view = view.mouse_pan(mouse_delta, extent.x / double(extent.y), 1.0);

			view = view.mouse_scroll(mouse_scroll);
		}
	}

	void Camera::config_ui() noexcept
	{
		float temp_near = projection.near, temp_fov = projection.fov_degrees;

		ImGui::SliderFloat("FOV", &temp_fov, 30.0f, 150.0f, "%.0f deg");
		ImGui::SliderFloat("Near Plane", &temp_near, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

		projection.fov_degrees = temp_fov;
		projection.near = temp_near;
	}

	render::Camera Camera::get(glm::u32vec2 extent) const noexcept
	{
		const auto aspect_ratio = static_cast<double>(extent.x) / static_cast<double>(extent.y);
		const auto camera_pos = view.view_position();
		const auto view_matrix = view.matrix();
		const auto proj_matrix = projection.matrix(aspect_ratio);
		const auto view_proj_matrix = scene::camera::reverse_z() * proj_matrix * view_matrix;

		return {
			.inv_view_projection = glm::inverse(view_proj_matrix),
			.prev_view_projection = view_proj_matrix,
			.view_projection = view_proj_matrix,
			.camera_pos = camera_pos,
			.prev_camera_pos = camera_pos,
		};
	}
}
