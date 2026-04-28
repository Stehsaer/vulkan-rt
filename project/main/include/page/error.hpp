#pragma once

#include "common/util/error.hpp"
#include "helper/imgui-page.hpp"
#include "resource/context.hpp"
#include "scene/page.hpp"
#include <memory>
#include <memory_resource>

namespace page
{
	///
	/// @brief Error displaying page
	///
	///
	class ErrorPage : public scene::Page
	{
	  public:

		///
		/// @brief Create an error page
		///
		/// @param context Vulkan context
		/// @param error Error content
		/// @param error_title Title of the error page
		/// @param hint_message Extra hint message
		/// @return Created error page or error
		///
		[[nodiscard]]
		static std::expected<ErrorPage, Error> from(
			resource::Context context,
			Error error,
			std::string error_title,
			std::string hint_message
		) noexcept;

		[[nodiscard]]
		std::expected<ResultType, Error> run_frame() noexcept override;

	  private:

		std::unique_ptr<resource::Context> context;
		helper::ImGuiPage imgui_page;
		Error error;
		std::string error_title;
		std::string hint_message;

		std::unique_ptr<std::pmr::unsynchronized_pool_resource> pool;

		explicit ErrorPage(
			std::unique_ptr<resource::Context> context,
			helper::ImGuiPage imgui_page,
			Error error,
			std::string error_title,
			std::string hint_message
		) :
			context(std::move(context)),
			imgui_page(std::move(imgui_page)),
			error(std::move(error)),
			error_title(std::move(error_title)),
			hint_message(std::move(hint_message)),
			pool(std::make_unique<std::pmr::unsynchronized_pool_resource>())
		{}

		// `true` if the page should quit after this frame, `false` otherwise
		[[nodiscard]]
		bool ui_logic() const noexcept;

	  public:

		ErrorPage(const ErrorPage&) = delete;
		ErrorPage(ErrorPage&&) = default;
		ErrorPage& operator=(const ErrorPage&) = delete;
		ErrorPage& operator=(ErrorPage&&) = default;
	};
}
