#pragma once

#include "argument.hpp"
#include "common/util/async.hpp"
#include "resource/context.hpp"
#include "scene/page.hpp"

#include <utility>

namespace page
{
	///
	/// @brief Starting state, creates basic vulkan context
	///
	class InitPage : public scene::Page
	{
	  public:

		///
		/// @brief Create an init page
		///
		/// @param argument Arguments input
		/// @return Created init page or error
		///
		[[nodiscard]]
		static InitPage from(Argument argument) noexcept;

		[[nodiscard]]
		std::expected<ResultType, Error> run_frame() noexcept override;

	  private:

		util::Future<std::expected<resource::Context, Error>> task;
		Argument argument;

		explicit InitPage(util::Future<std::expected<resource::Context, Error>> task, Argument argument) :
			task(std::move(task)),
			argument(std::move(argument))
		{}

	  public:

		InitPage(const InitPage&) = delete;
		InitPage(InitPage&&) = default;
		InitPage& operator=(const InitPage&) = delete;
		InitPage& operator=(InitPage&&) = default;
	};
}
