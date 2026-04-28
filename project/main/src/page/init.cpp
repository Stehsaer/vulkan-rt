#include "page/init.hpp"
#include "page/load.hpp"

namespace page
{
	InitPage InitPage::from(Argument argument) noexcept
	{
		auto task = util::Future(std::async(std::launch::async, [argument]() {
			return resource::Context::create();
		}));
		return InitPage(std::move(task), std::move(argument));
	}

	std::expected<InitPage::ResultType, Error> InitPage::run_frame() noexcept
	{
		if (!task.ready()) return ResultType::from<Result::Continue>();

		auto context_result = std::move(task).get();
		if (!context_result) return context_result.error().forward("Initialize context failed");
		auto context = std::move(*context_result);

		auto load_page_result = LoadPage::from(std::move(context), std::move(argument));
		if (!load_page_result) return load_page_result.error().forward("Initialize load page failed");

		return ResultType::from<Result::SwitchPage>(std::make_unique<LoadPage>(std::move(*load_page_result)));
	}
}
