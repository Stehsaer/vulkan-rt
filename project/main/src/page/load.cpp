#include "page/load.hpp"
#include "common/util/async.hpp"
#include "common/util/overload.hpp"
#include "helper/imgui-page.hpp"
#include "page/error.hpp"
#include "page/render.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"

#include <chrono>
#include <coro/thread_pool.hpp>
#include <imgui.h>
#include <print>
#include <tuple>

namespace page
{
	std::expected<render::Model, Error> LoadPage::load_model_task(
		const resource::Context& context,
		const render::MaterialLayout& material_layout,
		Argument argument,
		TaskProgress& progress
	) noexcept
	{
		const auto start_time = std::chrono::high_resolution_clock::now();

		const auto arg = std::move(argument);
		auto thread_pool = coro::thread_pool::make_unique();

		/* Load gltf model */

		auto [gltf_parsing_task, gltf_parsing_progress] =
			model::gltf::load_from_file(*thread_pool, std::filesystem::path(arg.model_path));
		progress.set<TaskProgressState::Parsing>(gltf_parsing_progress);
		auto gltf_parsing_result = coro::sync_wait(std::move(gltf_parsing_task));

		if (!gltf_parsing_result) return gltf_parsing_result.error().forward("Parse gltf model failed");
		auto gltf_model = std::move(*gltf_parsing_result);

		/* Load render model */

		const auto texture_load_opt = render::TextureList::LoadOption{
			.color_load_strategy = render::Texture::ColorLoadStrategy::BalancedBC,
			.exit_on_failed_load = true
		};

		auto [model_task, model_progress] = render::Model::create(
			*thread_pool,
			context.device.get(),
			material_layout,
			gltf_model,
			{.texture_load_option = texture_load_opt}
		);
		progress.set<TaskProgressState::Processing>(model_progress);

		auto model_loading_result = coro::sync_wait(std::move(model_task));
		if (!model_loading_result) return model_loading_result.error().forward("Load model failed");
		auto model = std::move(*model_loading_result);

		const auto end_time = std::chrono::high_resolution_clock::now();
		const std::chrono::duration<double> elapsed = end_time - start_time;
		std::println("Model loaded in {:.3f} seconds", elapsed.count());

		return model;
	}

	std::expected<LoadPage, Error> LoadPage::from(resource::Context context, Argument argument) noexcept
	{
		auto material_layout_result = render::MaterialLayout::create(context.device.get().device);
		if (!material_layout_result)
			return material_layout_result.error().forward("Create material layout failed");
		auto material_layout = std::make_unique<render::MaterialLayout>(std::move(*material_layout_result));

		auto imgui_page_result = helper::ImGuiPage::create(context.device.get());
		if (!imgui_page_result) return imgui_page_result.error().forward("Create ImGui page failed");
		auto imgui_page = std::move(*imgui_page_result);

		auto progress = std::make_unique<TaskProgress>(TaskProgress::from<TaskProgressState::Preparing>());
		auto context_res = std::make_unique<resource::Context>(std::move(context));

		auto model_future = std::async(
			std::launch::async,
			load_model_task,
			std::cref(*context_res),
			std::cref(*material_layout),
			std::move(argument),
			std::ref(*progress)
		);

		return LoadPage(
			std::move(context_res),
			std::move(imgui_page),
			Task{
				.material_layout = std::move(material_layout),
				.progress = std::move(progress),
				.model_future = util::Future(std::move(model_future)),
			}
		);
	}

	std::expected<LoadPage::ResultType, Error> LoadPage::run_frame() noexcept
	{
		switch (state_data.tag())
		{
		case State::Loading:
		{
			auto new_state_result = imgui_page.run_frame(*context, [this] {
				return ui(std::move(state_data).get<State::Loading>());
			});

			switch (new_state_result.tag())
			{
			case helper::ImGuiPage::ResultState::Success:
				state_data = std::move(new_state_result).get<helper::ImGuiPage::ResultState::Success>();
				return ResultType::from<Result::Continue>();

			case helper::ImGuiPage::ResultState::Wait:
				// ui not executed, `state_data` still valid
				return ResultType::from<Result::Continue>();

			case helper::ImGuiPage::ResultState::Quit:
				std::ignore = std::move(state_data).get<State::Loading>().model_future.get();
				context->device->waitIdle();
				return ResultType::from<Result::Quit>();

			case helper::ImGuiPage::ResultState::Error:
				context->device->waitIdle();
				return std::move(new_state_result).get<helper::ImGuiPage::ResultState::Error>();

			default:
				return Error("Invalid state");
			}
		}
		case State::Error:
		{
			context->device->waitIdle();
			auto error_page_result = ErrorPage::from(
				std::move(*context),
				state_data.get<State::Error>(),
				"Load model failed",
				"Check the model file."
			);
			if (!error_page_result) return error_page_result.error().forward("Create error page failed");

			return ResultType::from<Result::SwitchPage>(
				std::make_unique<ErrorPage>(std::move(*error_page_result))
			);
		}

		case State::Quiting:
			context->device->waitIdle();
			return state_data.get<State::Quiting>();

		case State::Success:
		{
			auto& success_data = state_data.get<State::Success>();
			context->device->waitIdle();

			auto render_page_result = RenderPage::create(
				std::move(*context),
				std::move(success_data.model),
				std::move(*success_data.material_layout)
			);
			if (!render_page_result) return render_page_result.error().forward("Create render page failed");

			return ResultType::from<Result::SwitchPage>(
				std::make_unique<RenderPage>(std::move(*render_page_result))
			);
		}
		default:
			return Error("Invalid state");
		}
	}

	static void begin_centered_window(const std::string& title)
	{
		const auto* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints({0, 0}, {viewport->Size.x / 2, viewport->Size.y / 2});

		ImGui::Begin(
			title.c_str(),
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize
				| ImGuiWindowFlags_NoTitleBar
				| ImGuiWindowFlags_NoMove
				| ImGuiWindowFlags_NoResize
				| ImGuiWindowFlags_NoSavedSettings
		);
	}

	template <auto S>
	static void progress_bar_impl(const util::Tag<S>&) noexcept
	{
		ImGui::ProgressBar(-1.0f * ImGui::GetTime());
	}

	template <auto S>
	static void progress_bar_impl(const util::Tag<S, util::Progress::Ref>& state) noexcept
	{
		ImGui::ProgressBar(state.value.get_progress());
	}

	static constexpr auto progress_bar_lambda = [](auto&& state) {
		progress_bar_impl(state);
	};

	static const char* get_gltf_progress_text(model::gltf::ProgressState state) noexcept
	{
		switch (state)
		{
		case model::gltf::ProgressState::Parsing:
			return "Parsing gltf model...";
		case model::gltf::ProgressState::Material:
			return "Locating materials...";
		case model::gltf::ProgressState::Mesh:
			return "Building meshes...";
		case model::gltf::ProgressState::Hierarchy:
			return "Building hierarchy...";
		default:
			return "Unknown progress state";
		}
	}

	static const char* get_render_progress_text(render::Model::ProgressState state) noexcept
	{
		switch (state)
		{
		case render::Model::ProgressState::Preparing:
			return "";
		case render::Model::ProgressState::Mesh:
			return "Processing meshes...";
		case render::Model::ProgressState::Material:
			return "Processing materials...";
		default:
			UNREACHABLE("Invalid progress state", state);
		}
	}

	LoadPage::StateData LoadPage::ui(Task task) noexcept
	{
		begin_centered_window("Loading");
		switch (task.progress->tag())
		{
		case TaskProgressState::Preparing:
			ImGui::Text("Preparing...");
			ImGui::ProgressBar(-1.0f * ImGui::GetTime());
			break;

		case TaskProgressState::Parsing:
		{
			const auto& state = *task.progress->get<TaskProgressState::Parsing>();
			ImGui::Text(
				"(%zu/%zu) %s",
				state.index() + 1,
				state.tag_count(),
				get_gltf_progress_text(state.tag())
			);
			state.visit(progress_bar_lambda);
			break;
		}
		case TaskProgressState::Processing:
		{
			const auto& state = *task.progress->get<TaskProgressState::Processing>();
			ImGui::Text(
				"(%zu/%zu) %s",
				state.index() + 1,
				state.tag_count(),
				get_render_progress_text(state.tag())
			);
			state.visit(
				util::Overload(
					[]<auto S>(const util::Tag<S>& state) { progress_bar_lambda(state); },
					[]<auto S, typename T>(const util::Tag<S, std::shared_ptr<T>>& state) {
						state.value->visit(progress_bar_lambda);
					}
				)
			);
			break;
		}
		}
		ImGui::End();

		if (task.model_future.ready())
		{
			auto result = std::move(task.model_future).get();
			if (!result) return StateData::from<State::Error>(result.error());

			return StateData::from<State::Success>(
				{.model = std::move(*result), .material_layout = std::move(task.material_layout)}
			);
		}

		return StateData::from<State::Loading>(std::move(task));
	}
}
