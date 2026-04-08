#pragma once

#include "common/util/error.hpp"
#include "common/util/span.hpp"

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

	template <std::ranges::contiguous_range T>
	std::expected<void, Error> write(
		const std::filesystem::path& path,
		T&& data,
		WriteMode mode = WriteMode::Overwrite
	) noexcept
	{
		const auto byte_span = util::as_bytes(std::span(data));
		return write(path, byte_span, mode);
	}

	[[nodiscard]]
	std::expected<void, Error> write(
		const std::filesystem::path& path,
		std::span<const std::byte> data,
		WriteMode mode = WriteMode::Overwrite
	) noexcept;
}
