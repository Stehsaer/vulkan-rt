#pragma once

#include "buffer.hpp"
#include "file-cache.hpp"

#include <cstddef>
#include <fastgltf/types.hpp>

namespace model::gltf::impl
{
	// Augmented asset structure
	struct Asset : public fastgltf::Asset
	{
		std::optional<std::filesystem::path> directory;
		std::shared_ptr<FileCache> file_cache;
		std::vector<Buffer> unified_buffers;

		[[nodiscard]]
		static std::expected<Asset, Error> create(
			fastgltf::Asset asset,
			std::optional<std::filesystem::path> directory = std::nullopt
		) noexcept;

		// Used in accessor tools
		[[nodiscard]]
		std::span<const std::byte> accessor_interface(
			const fastgltf::Asset&,
			size_t buffer_view_idx
		) noexcept;

	  private:

		explicit Asset(
			fastgltf::Asset asset,
			std::optional<std::filesystem::path> directory,
			std::shared_ptr<FileCache> file_cache,
			std::vector<Buffer> unified_buffers
		) :
			fastgltf::Asset(std::move(asset)),
			directory(std::move(directory)),
			file_cache(std::move(file_cache)),
			unified_buffers(std::move(unified_buffers))
		{}

	  public:

		Asset(const Asset&) = delete;
		Asset(Asset&&) = default;
		Asset& operator=(const Asset&) = delete;
		Asset& operator=(Asset&&) = default;
	};
}
