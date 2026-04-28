#include "logic/param/auto-exposure.hpp"

#include <imgui.h>

namespace logic
{
	void Exposure::config_ui() noexcept
	{
		ImGui::SliderFloat(
			"Adaptation Rate",
			&adaptation_rate,
			0.1f,
			10.0f,
			"%.1f",
			ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp
		);

		ImGui::SliderFloat("Exposure", &exposure_ev, -5.0f, 5.0f, "%.1f EV", ImGuiSliderFlags_AlwaysClamp);
	}

	render::ExposureParam Exposure::get(float dt, glm::u32vec2 extent) const noexcept
	{
		return {
			.min_log_luminance = glm::log(1.0e-5f),
			.max_log_luminance = glm::log(1.0e3f),
			.delta_time = dt,
			.exposure_mult = glm::pow(2.0f, exposure_ev),
			.adaptation_rate = adaptation_rate,
			.image_size = extent,
		};
	}
}
