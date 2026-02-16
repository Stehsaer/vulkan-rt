#include <SDL3/SDL_events.h>
#include <iostream>
#include <print>
#include <ranges>

#include "app.hpp"
#include "argument.hpp"
#include "common/util/error.hpp"

int main(int argc, const char* argv[]) noexcept
{
	Argument argument;

	auto argument_parse_result = Argument::parse(std::span<const char*>(argv, argc));
	if (argument_parse_result)
	{
		argument = std::move(*argument_parse_result);
	}
	else
	{
		std::string model_path;

		std::print(std::cerr, "Manually input model path: ");
		std::getline(std::cin, model_path);

		argument.model_path = std::move(model_path);
	}

	try
	{
		App app = App::create(argument);
		while (app.draw_frame());
		return 0;
	}
	catch (const Error& error)
	{
		std::println(std::cerr, "\x1b[91mError\x1b[0m: {:msg}", error);
		for (const auto& [idx, entry] : std::views::enumerate(error.chain()))
			std::println(std::cerr, "[#{}]: {}", idx, entry);

		return 1;
	}
	catch (const std::exception& e)
	{
		std::println(std::cerr, "\x1b[91mError\x1b[0m: an unexpected error occurred: {}", e.what());
		return 1;
	}
}
