#include "file-cache.hpp"

namespace model::gltf::impl
{
	std::expected<std::shared_ptr<mio::basic_mmap_source<std::byte>>, Error> FileCache::get(
		const std::filesystem::path& path
	) noexcept
	{
		const std::scoped_lock lock(*mutex);

		auto it = cache.find(path);
		if (it != cache.end()) return it->second;

		if (!std::filesystem::exists(path))
			return Error("File doesn't exist", std::format("Path: {}", path.string()));
		if (!std::filesystem::is_regular_file(path))
			return Error("Path is not a regular file", std::format("Path: {}", path.string()));

		try
		{
			auto shared_mmap = std::make_shared<mio::basic_mmap_source<std::byte>>(path.string(), 0);
			cache.emplace(path, shared_mmap);

			return shared_mmap;
		}
		catch (const std::system_error& e)
		{
			return Error(
				"Memory map file failed",
				std::format("Path: {}, what(): {:?}", path.string(), e.what())
			);
		}
	}
}
