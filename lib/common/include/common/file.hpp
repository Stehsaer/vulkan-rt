#pragma once

#include "common/util/error.hpp"

#include <filesystem>
#include <span>

namespace file
{
	[[nodiscard]]
	std::expected<std::vector<std::byte>, Error> read(
		const std::filesystem::path& path,
		size_t size_limit = 1024 * 1024 * 1024
	) noexcept;

	enum class WriteMode
	{
		Overwrite,
		Append
	};

	[[nodiscard]]
	std::expected<void, Error> write(
		const std::filesystem::path& path,
		std::span<const std::byte> data,
		WriteMode mode = WriteMode::Overwrite
	) noexcept;
}
