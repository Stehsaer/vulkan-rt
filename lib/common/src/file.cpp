#include "common/file.hpp"

#include <fstream>
#include <utility>

namespace file
{
	static_assert(sizeof(std::byte) == sizeof(char), "std::byte must have the same size as char");

	std::expected<std::vector<std::byte>, Error> read(
		const std::filesystem::path& path,
		size_t size_limit
	) noexcept
	{
		std::ifstream file;
		file.exceptions(std::ios::failbit | std::ios::badbit);

		try
		{
			file.open(path, std::ios::binary | std::ios::ate);
			const std::make_signed_t<size_t> file_size = file.tellg();

			if (file_size < 0)
				return Error(
					std::format("Get size of file '{}' failed", path.string()),
					"File size is negative"
				);

			if (std::cmp_greater(file_size, size_limit))
				return Error(
					std::format("Read file '{}' failed: file size exceeds limit", path.string()),
					std::format("File size: {}, limit: {}", file_size, size_limit)
				);

			std::vector<std::byte> data(static_cast<size_t>(file_size));
			file.seekg(0);
			file.read(reinterpret_cast<char*>(data.data()), file_size);

			file.close();

			return data;
		}
		catch (const std::ios::failure& e)
		{
			return Error(std::format("Read file '{}' failed", path.string()), e.what());
		}
	}

	std::expected<void, Error> write(
		const std::filesystem::path& path,
		std::span<const std::byte> data,
		WriteMode mode
	) noexcept
	{
		std::ofstream file;
		file.exceptions(std::ios::failbit | std::ios::badbit);

		std::ios::openmode open_mode = std::ios::binary;
		if (mode == WriteMode::Append) open_mode |= std::ios::app;

		try
		{
			file.open(path, open_mode);
			file.write(reinterpret_cast<const char*>(data.data()), data.size());
			file.close();

			return {};
		}
		catch (const std::ios::failure& e)
		{
			return Error(std::format("Write file '{}' failed", path.string()), e.what());
		}
	}
}
