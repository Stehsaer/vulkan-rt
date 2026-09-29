#include <cstdlib>
#include <iostream>
#include <libassert/assert.hpp>
#include <nlohmann/json.hpp>  // IWYU pragma: keep
#include <nlohmann/json_fwd.hpp>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <utility>

#include "argument.hpp"
#include "common/container/debug-trace.hpp"
#include "common/container/debug-value.hpp"
#include "common/container/error.hpp"
#include "common/file.hpp"
#include "common/util/span.hpp"
#include "page/init.hpp"
#include "scene/page.hpp"

static Argument get_arguments(int argc, const char* argv[]) noexcept
{
	auto argument_parse_result = Argument::parse(std::span<const char*>(argv, argc));
	if (argument_parse_result) return std::move(*argument_parse_result);

	std::string model_path;

	std::print(std::cerr, "Manually input model path: ");
	std::getline(std::cin, model_path);

	return Argument{.model_path = std::move(model_path)};
}

static nlohmann::json summarize_error(const Error& e) noexcept
{
	using debug::operator""_key;

	const debug::Trace trace = {
		"errors"_key =
			e.chain() | std::views::transform([](const auto& err) { return debug::value::Error(err); })
	};

	return trace.format<nlohmann::json>();
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
			std::println("Error: {}", result.error().root());

			auto error_dump = summarize_error(result.error()).dump();

			if (!file::write("error.json", util::as_bytes(error_dump)))
			{
				std::println("Dump error to file failed, writing to stdout.");
				std::println("{}", error_dump);
			}
			else
			{
				std::println("Error dumped to error.json");
			}

			return EXIT_FAILURE;
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
