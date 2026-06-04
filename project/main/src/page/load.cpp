#include "page/load.hpp"
#include "argument.hpp"
#include "common/util/async.hpp"
#include "common/util/error.hpp"
#include "common/util/overload.hpp"
#include "common/util/tagged-type.hpp"
#include "helper/imgui-page.hpp"
#include "model/gltf.hpp"
#include "page/error.hpp"
#include "page/render.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"
#include "render/model/texture-list.hpp"
#include "render/model/texture.hpp"
#include "render/model/tlas.hpp"
#include "resource/context.hpp"

#include <chrono>
#include <coro/sync_wait.hpp>
#include <coro/thread_pool.hpp>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <glm/ext/matrix_float4x4.hpp>
#include <imgui.h>
#include <libassert/assert.hpp>
#include <memory>
#include <print>
#include <string>
#include <tuple>
#include <utility>
#include <vulkan/vulkan.hpp>

namespace page
{
	std::expected<std::tuple<render::Model, render::Tlas>, Error> LoadPage::load_model_task(
		std::shared_ptr<const resource::Context> context,  // NOLINT: intended to own
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
			context->device.get(),
			material_layout,
			gltf_model,
			{.texture_load_option = texture_load_opt}
		);
		progress.set<TaskProgressState::Processing>(model_progress);

		auto model_loading_result = coro::sync_wait(std::move(model_task));
		if (!model_loading_result) return model_loading_result.error().forward("Load model failed");
		auto model = std::move(*model_loading_result);

		const auto transforms = model.hierarchy.compute_transforms(glm::mat4(1.0));
		auto tlas_result = render::Tlas::build(context->device.get(), model, transforms);
		if (!tlas_result) return tlas_result.error().forward("Create TLAS for scene failed");
		auto tlas = std::move(*tlas_result);

		const auto end_time = std::chrono::high_resolution_clock::now();
		const std::chrono::duration<double> elapsed = end_time - start_time;
		std::println("Model loaded in {:.3f} seconds", elapsed.count());

		return std::make_tuple(std::move(model), std::move(tlas));
	}

	std::expected<LoadPage, Error> LoadPage::from(resource::Context context, Argument argument) noexcept
	{
		auto material_layout_result = render::MaterialLayout::create(context.device.get());
		if (!material_layout_result)
			return material_layout_result.error().forward("Create material layout failed");
		auto material_layout = std::make_unique<render::MaterialLayout>(std::move(*material_layout_result));

		auto imgui_page_result =
			helper::ImGuiPage::create(context.device.get(), context.swapchain->image_count);
		if (!imgui_page_result) return imgui_page_result.error().forward("Create ImGui page failed");
		auto imgui_page = std::move(*imgui_page_result);

		auto progress = std::make_unique<TaskProgress>(TaskProgress::from<TaskProgressState::Preparing>());
		auto context_res = std::make_shared<resource::Context>(std::move(context));

		auto model_future = std::async(
			std::launch::async,
			load_model_task,
			context_res,
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
				if (const auto result = context->device->waitIdle(); !result) return Error::from(result);
				return ResultType::from<Result::Quit>();

			case helper::ImGuiPage::ResultState::Error:
				if (const auto result = context->device->waitIdle(); !result) return Error::from(result);
				return std::move(new_state_result).get<helper::ImGuiPage::ResultState::Error>();

			default:
				UNREACHABLE("Invalid state");
			}
		}
		case State::Error:
		{
			if (const auto result = context->device->waitIdle(); !result) return Error::from(result);
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
			if (const auto result = context->device->waitIdle(); !result) return Error::from(result);
			return state_data.get<State::Quiting>();

		case State::Success:
		{
			auto& success_data = state_data.get<State::Success>();
			if (const auto result = context->device->waitIdle(); !result) return Error::from(result);

			auto render_page_result = RenderPage::create(
				context,
				std::move(*success_data.material_layout),
				std::move(success_data.model),
				std::move(success_data.tlas)
			);
			if (!render_page_result) return render_page_result.error().forward("Create render page failed");

			return ResultType::from<Result::SwitchPage>(
				std::make_unique<RenderPage>(std::move(*render_page_result))
			);
		}
		default:
			UNREACHABLE("Invalid state");
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
	static void progress_bar_impl(const util::Tag<S, util::ProgressRef>& state) noexcept
	{
		ImGui::ProgressBar(state.value.get_progress().value_or(0.0));
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
			UNREACHABLE("Invalid state");
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
		case render::Model::ProgressState::Blas:
			return "Creating BLAS...";
		default:
			UNREACHABLE("Invalid state", state);
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

			auto [model, tlas] = std::move(*result);

			return StateData::from<State::Success>({
				.model = std::move(model),
				.tlas = std::move(tlas),
				.material_layout = std::move(task.material_layout),
			});
		}

		return StateData::from<State::Loading>(std::move(task));
	}
}
