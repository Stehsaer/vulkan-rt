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
#ifndef NDEBUG
		std::string model_path;

		std::print(std::cerr, "Manually input model path: ");
		std::getline(std::cin, model_path);

		argument.model_path = std::move(model_path);
#else
		std::println(std::cerr, "{}", argument_parse_result.error().message);
		std::println(std::cerr, "{}", argument_parse_result.error().detail.value_or(""));
		return 1;
#endif
	}

	try
	{
		App app = App::create(argument);

		bool quit = false;
		while (!quit)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_EVENT_QUIT:
					quit = true;
					break;
				case SDL_EVENT_WINDOW_MINIMIZED:
					continue;
				default:
					break;
				}
			}

			app.draw_frame();
		}

		return 0;
	}
	catch (const Error& error)
	{
		try
		{
			std::println(std::cerr, "\x1b[91mError\x1b[0m: {:msg}", error);
			for (const auto& [idx, entry] : std::views::enumerate(error.chain()))
				std::println(std::cerr, "[#{}]: {}", idx, entry);
		}
		catch (const std::exception& err)
		{
			std::cerr
				<< "\x1b[91mError\x1b[0m: an error occurred, but formatting the error has failed: "
				<< err.what()
				<< '\n';
		}

		return 1;
	}
	catch (const std::exception& e)
	{
		std::cerr << "\x1b[91mError\x1b[0m: an unexpected error occurred: " << e.what() << '\n';
		return 1;
	}
}
