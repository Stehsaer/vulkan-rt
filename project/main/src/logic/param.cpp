#include "logic/param.hpp"

#include <imgui.h>

namespace logic
{
	void Param::ui(glm::u32vec2 extent) noexcept
	{
		camera.update(extent);

		if (ImGui::Begin("Render Settings"))
		{
			ImGui::SeparatorText("Light");
			primary_light.config_ui();

			ImGui::SeparatorText("Exposure");
			exposure.config_ui();

			ImGui::SeparatorText("Camera");
			camera.config_ui();
		}
		ImGui::End();
	}
}
