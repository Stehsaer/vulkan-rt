#include "argument.hpp"
#include "common/util/error.hpp"

#include <argparse/argparse.hpp>
#include <exception>
#include <expected>
#include <span>

std::expected<Argument, Error> Argument::parse(std::span<const char*> arguments) noexcept
{
	Argument argument;

	argparse::ArgumentParser parser("main");
	parser.add_argument("model")
		.help("Path to the model to render")
		.required()
		.store_into(argument.model_path);

	try
	{
		parser.parse_args(arguments.size(), arguments.data());
	}
	catch (const std::exception& e)
	{
		return Error(e.what(), parser.usage());
	}

	return argument;
}
