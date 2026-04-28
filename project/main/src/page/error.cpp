#include "page/error.hpp"
#include <imgui.h>

namespace page
{
	std::expected<ErrorPage, Error> ErrorPage::from(
		resource::Context context,
		Error error,
		std::string error_title,
		std::string hint_message
	) noexcept
	{
		auto image_page_result = helper::ImGuiPage::create(context.device.get());
		if (!image_page_result) return image_page_result.error().forward("Create ImGui page failed");
		auto image_page = std::move(*image_page_result);

		return ErrorPage(
			std::make_unique<resource::Context>(std::move(context)),
			std::move(image_page),
			std::move(error),
			std::move(error_title),
			std::move(hint_message)
		);
	}

	std::expected<ErrorPage::ResultType, Error> ErrorPage::run_frame() noexcept
	{
		const auto ui_result = imgui_page.run_frame(*context, [this] { return ui_logic(); });

		switch (ui_result.tag())
		{
		case helper::ImGuiPage::ResultState::Success:
		{
			const auto quit = ui_result.get<helper::ImGuiPage::ResultState::Success>();

			if (quit)
			{
				context->device->waitIdle();
				return ResultType::from<Result::Quit>();
			}
			else
			{
				return ResultType::from<Result::Continue>();
			}
		}
		case helper::ImGuiPage::ResultState::Quit:
			context->device->waitIdle();
			return error.forward("User quit from error page");

		case helper::ImGuiPage::ResultState::Error:
			context->device->waitIdle();
			return ui_result.get<helper::ImGuiPage::ResultState::Error>();

		default:
			return Error("Invalid state");
		}
	}

	bool ErrorPage::ui_logic() const noexcept
	{
		const auto viewport_size = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos({viewport_size.x / 2, viewport_size.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(
			{viewport_size.x / 3, 0},
			{viewport_size.x / 4 * 3, viewport_size.y / 4 * 3}
		);

		ImGui::PushStyleVarX(ImGuiStyleVar_WindowTitleAlign, 0.5);
		ImGui::Begin(
			error_title.c_str(),
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
		);

		ImGui::TextWrapped("%s", hint_message.c_str());
		ImGui::Separator();
		if (ImGui::TreeNode("Details"))
		{
			for (const auto& [idx, error] : std::views::enumerate(error.chain()))
			{
				std::pmr::string error_message(pool.get());
				std::format_to(std::back_inserter(error_message), "[#{}] {:msg}\n", idx, error);
				ImGui::BulletText("%s", error_message.c_str());
			}
			ImGui::TreePop();
		}

		ImGui::End();
		ImGui::PopStyleVar();

		return false;
	}
}
