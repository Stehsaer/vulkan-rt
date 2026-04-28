#include "scene/page.hpp"

namespace scene
{
	std::expected<void, Error> Page::run(std::unique_ptr<Page> initial_page) noexcept
	{
		if (initial_page == nullptr) return Error("Initial page must not be null");
		std::unique_ptr<Page> current_page = std::move(initial_page);

		while (true)
		{
			auto result = current_page->run_frame();
			if (!result) return result.error();

			switch (result->tag())
			{
			case Page::Result::Continue:
				continue;

			case Page::Result::SwitchPage:
			{
				auto new_page = std::move(*result).get<Page::Result::SwitchPage>();
				if (new_page == nullptr) return Error("New page must not be null");
				current_page = std::move(new_page);
				break;
			}
			case Page::Result::Quit:
				return {};

			default:
				return Error("Invalid page result");
			}
		}

		return {};
	}
}
