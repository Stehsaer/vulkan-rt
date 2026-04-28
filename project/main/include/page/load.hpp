#pragma once

#include "argument.hpp"
#include "common/util/tagged-type.hpp"
#include "helper/imgui-page.hpp"
#include "model/gltf.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"
#include "resource/context.hpp"
#include "scene/page.hpp"

#include <vulkan/vulkan_raii.hpp>

namespace page
{
	///
	/// @brief Page showing ongoing loading progress
	///
	///
	class LoadPage : public scene::Page
	{
	  public:

		///
		/// @brief Create a loading page
		///
		/// @param context Vulkan context
		/// @param argument Argument input
		/// @return Created load page or error
		///
		[[nodiscard]]
		static std::expected<LoadPage, Error> from(resource::Context context, Argument argument) noexcept;

		[[nodiscard]]
		std::expected<ResultType, Error> run_frame() noexcept override;

	  private:

		enum class TaskProgressState
		{
			Preparing,
			Parsing,
			Processing
		};

		enum class State
		{
			Loading,
			Error,
			Quiting,
			Success
		};

		using TaskProgress = util::SyncedEnumVariant<
			util::Tag<TaskProgressState::Preparing>,
			util::Tag<TaskProgressState::Parsing, std::shared_ptr<const model::gltf::Progress>>,
			util::Tag<TaskProgressState::Processing, std::shared_ptr<const render::Model::Progress>>
		>;

		struct Task
		{
			util::Future<std::expected<render::Model, Error>> model_future;
			std::unique_ptr<TaskProgress> progress;
			std::unique_ptr<render::MaterialLayout> material_layout;
		};

		struct TaskResult
		{
			render::Model model;
			std::unique_ptr<render::MaterialLayout> material_layout;
		};

		using StateData = util::EnumVariant<
			util::Tag<State::Loading, Task>,
			util::Tag<State::Error, Error>,
			util::Tag<State::Success, TaskResult>,
			util::Tag<State::Quiting, Error>
		>;

		std::unique_ptr<resource::Context> context;
		helper::ImGuiPage imgui_page;
		StateData state_data;

		static std::expected<render::Model, Error> load_model_task(
			const resource::Context& context,
			const render::MaterialLayout& material_layout,
			Argument argument,
			TaskProgress& progress
		) noexcept;

		static StateData ui(Task task) noexcept;

		/*===== Construct =====*/

		explicit LoadPage(
			std::unique_ptr<resource::Context> context,
			helper::ImGuiPage imgui_page,
			Task task
		) :
			context(std::move(context)),
			imgui_page(std::move(imgui_page)),
			state_data(util::tag_value<State::Loading>(std::move(task)))
		{}

	  public:

		LoadPage(const LoadPage&) = delete;
		LoadPage(LoadPage&&) = default;
		LoadPage& operator=(const LoadPage&) = delete;
		LoadPage& operator=(LoadPage&&) = default;
	};
}
