#pragma once

#include "common/util/error.hpp"

#include <filesystem>
#include <memory>
#include <mio/mmap.hpp>
#include <mutex>
#include <unordered_map>

namespace model::gltf::impl
{
	// File cache, maps the file efficiently
	class FileCache
	{
	  public:

		FileCache() :
			mutex(std::make_unique<std::mutex>())
		{}

		[[nodiscard]]
		std::expected<std::shared_ptr<mio::basic_mmap_source<std::byte>>, Error> get(
			const std::filesystem::path& path
		) noexcept;

	  private:

		std::unique_ptr<std::mutex> mutex;
		std::unordered_map<std::filesystem::path, std::shared_ptr<mio::basic_mmap_source<std::byte>>> cache;

	  public:

		FileCache(const FileCache&) = delete;
		FileCache(FileCache&&) = default;
		FileCache& operator=(const FileCache&) = delete;
		FileCache& operator=(FileCache&&) = default;
	};
}
