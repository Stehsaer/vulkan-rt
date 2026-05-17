#include <SDL3/SDL_events.h>
#include <iostream>
#include <libassert/assert.hpp>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <utility>

#include "argument.hpp"
#include "common/util/error.hpp"
#include "page/init.hpp"
#include "scene/page.hpp"

static Argument get_arguments(int argc, const char* argv[])
{
	auto argument_parse_result = Argument::parse(std::span<const char*>(argv, argc));
	if (argument_parse_result) return std::move(*argument_parse_result);

	std::string model_path;

	std::print(std::cerr, "Manually input model path: ");
	std::getline(std::cin, model_path);

	return Argument{.model_path = std::move(model_path)};
}

int main(int argc, const char* argv[]) noexcept
{
	const auto argument = get_arguments(argc, argv);

	auto page = page::InitPage::from(argument).as_ptr();

	while (true)
	{
		auto result = page->run_frame();

		if (!result)
		{
			std::println(std::cerr, "\x1b[91;1mError\x1b[0m: {:msg}", result.error().root());

			for (const auto& [idx, entry] : std::views::enumerate(result.error().chain()))
			{
				std::println(std::cerr, "\x1b[96;1m[#{}]\x1b[0m: {:msg}", idx, entry);
				if (entry.detail.has_value())
					std::println(std::cerr, "    - \x1b[1mDetail\x1b[0m: {:detail}", entry);
				std::println(std::cerr, "    - \x1b[1;4mSource\x1b[0m: {:loc}", entry);
			}

			return 1;
		}

		auto next = std::move(*result);

		switch (next.tag())
		{
		case scene::Page::Result::Continue:
			continue;
		case scene::Page::Result::SwitchPage:
			page = std::move(next.get<scene::Page::Result::SwitchPage>());
			continue;
		case scene::Page::Result::Quit:
			return 0;
		default:
			UNREACHABLE("Invalid result");
		}
	}
}
