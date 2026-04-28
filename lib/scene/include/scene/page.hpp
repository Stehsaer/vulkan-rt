#pragma once

#include "common/util/error.hpp"
#include "common/util/tagged-type.hpp"

#include <memory>

namespace scene
{
	///
	/// @brief An abstract page class
	///
	///
	class Page
	{
	  public:

		enum class Result
		{
			Continue,    // Continue running the current page
			SwitchPage,  // Switch to a new page provided in the result
			Quit         // Quit the page runner
		};

		using ResultType = util::EnumVariant<
			util::Tag<Result::Continue>,
			util::Tag<Result::SwitchPage, std::unique_ptr<Page>>,
			util::Tag<Result::Quit>
		>;

		///
		/// @brief Run one frame of the page
		/// @note When running a frame, throwing an error is permitted. Such errors will be caught by the page
		/// runner and treated as a page result of type `Error`
		///
		/// @return The result of running the page for one frame, indicating whether to continue, switch page,
		/// report an error, or quit.
		///
		virtual std::expected<ResultType, Error> run_frame() noexcept = 0;

		virtual ~Page() noexcept = default;

		///
		/// @brief Run page loops, with a designated initial page. It keeps running until the page loop quits
		/// or an error occurs.
		///
		/// @param initial_page The initial page to start running. Must not be empty.
		/// @retval void Success, quitting the page runner
		/// @retval Error An error occurred during page execution, with details provided in the error
		///
		static std::expected<void, Error> run(std::unique_ptr<Page> initial_page) noexcept;

		///
		/// @brief Convert the page to a unique pointer.
		///
		/// @return A unique pointer to the page instance.
		///
		std::unique_ptr<Page> as_ptr(this auto&& self) noexcept
		{
			return std::make_unique<std::remove_cvref_t<decltype(self)>>(std::forward<decltype(self)>(self));
		}
	};
}
