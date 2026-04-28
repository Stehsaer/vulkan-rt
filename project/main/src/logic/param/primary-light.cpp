#include "logic/param/primary-light.hpp"

#include <imgui.h>

namespace logic
{
	void PrimaryLight::config_ui() noexcept
	{
		ImGui::SliderFloat("Yaw", &light_yaw_deg, 0.0f, 360.0f);
		ImGui::SliderFloat("Pitch", &light_pitch_deg, 0.0f, 90.0f);
		ImGui::ColorEdit3("Color", &light_color.r);
		ImGui::SliderFloat(
			"Intensity",
			&light_intensity,
			0.01f,
			100.0f,
			"%.4g",
			ImGuiSliderFlags_Logarithmic
		);
	}

	render::DirectLight PrimaryLight::get() const noexcept
	{
		const auto light_yaw = glm::radians(light_yaw_deg);
		const auto light_pitch = glm::radians(light_pitch_deg);

		const auto direction = glm::vec3(
			glm::cos(light_yaw) * glm::cos(light_pitch),
			glm::sin(light_pitch),
			glm::sin(light_yaw) * glm::cos(light_pitch)
		);

		const auto light = light_color * light_intensity;

		return render::DirectLight{
			.direction = direction,
			.light = light,
			.sin_half_angle = 0.0f,
		};
	}
}
