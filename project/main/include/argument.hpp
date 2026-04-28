#pragma once

#include "common/util/error.hpp"

#include <expected>
#include <span>
#include <string>

struct Argument
{
	std::string model_path;

	///
	/// @brief Parse the argument
	///
	/// @param arguments Input argument
	/// @return Parsed argument or error
	///
	[[nodiscard]]
	static std::expected<Argument, Error> parse(std::span<const char*> arguments) noexcept;
};
