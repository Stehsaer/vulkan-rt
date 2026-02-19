#pragma once

#include "common/util/error.hpp"

#include <expected>
#include <span>
#include <string>

struct Argument
{
	std::string model_path;

	static std::expected<Argument, Error> parse(std::span<const char*> arguments) noexcept;
};
