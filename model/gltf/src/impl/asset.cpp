#include "asset.hpp"
#include <ranges>
#include <utility>

namespace model::gltf::impl
{
	std::expected<Asset, Error> Asset::create(
		fastgltf::Asset asset,
		std::optional<std::filesystem::path> directory
	) noexcept
	{
		auto file_cache = std::make_shared<FileCache>();

		auto buffers_result =
			asset.buffers
			| std::views::transform([&directory, &file_cache](const fastgltf::Buffer& buffer) {
				  return Buffer::create(buffer, file_cache, directory);
			  })
			| Error::collect();
		if (!buffers_result) return buffers_result.error().forward("Create buffers failed");
		auto buffers = std::move(*buffers_result);

		return Asset(std::move(asset), std::move(directory), file_cache, std::move(buffers));
	}

	std::span<const std::byte> Asset::accessor_interface(
		const fastgltf::Asset&,
		size_t buffer_view_idx
	) noexcept
	{
		const auto& buffer_view = bufferViews[buffer_view_idx];
		return unified_buffers[buffer_view.bufferIndex].get_span(buffer_view).value();
	}
}
