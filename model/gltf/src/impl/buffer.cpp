#include "buffer.hpp"
#include "common/util/overload.hpp"

#include <libassert/assert.hpp>
#include <utility>

namespace model::gltf::impl
{
	[[nodiscard]]
	std::expected<Buffer::Data, Error> Buffer::get_data(
		const std::monostate&,
		const std::optional<std::filesystem::path>&
	) noexcept
	{
		return Error("Buffer has no source");
	}

	[[nodiscard]]
	std::expected<Buffer::Data, Error> Buffer::get_data(
		const fastgltf::sources::BufferView&,
		const std::optional<std::filesystem::path>&
	) noexcept
	{
		return Error("Buffer views mustn't be used as buffer sources");
	}

	[[nodiscard]]
	std::expected<Buffer::Data, Error> Buffer::get_data(
		const fastgltf::sources::URI& uri,
		const std::optional<std::filesystem::path>& directory
	) noexcept
	{
		if (!uri.uri.isLocalPath())
		{
			return Error(
				"Only local paths are supported as buffer sources",
				std::format("URI: {}", uri.uri.string())
			);
		}

		auto path = uri.uri.fspath();

		if (path.is_relative())
		{
			if (!directory)
			{
				return Error(
					"Relative buffer source URI provided but asset has no directory",
					std::format("URI: {}", uri.uri.string())
				);
			}

			path = *directory / path;
		}

		if (!std::filesystem::exists(path))
			return Error("Buffer source file doesn't exist", std::format("Path: {}", path.string()));

		if (!std::filesystem::is_regular_file(path))
			return Error("Buffer source path is not a regular file", std::format("Path: {}", path.string()));

		return path;
	}

	[[nodiscard]]
	std::expected<Buffer::Data, Error> Buffer::get_data(
		const fastgltf::sources::Array& array,
		const std::optional<std::filesystem::path>&
	) noexcept
	{
		return std::make_unique<std::vector<std::byte>>(std::from_range, array.bytes);
	}

	[[nodiscard]]
	std::expected<Buffer::Data, Error> Buffer::get_data(
		const fastgltf::sources::Vector& vector,
		const std::optional<std::filesystem::path>&
	) noexcept
	{
		return vector.bytes;  // copy is intended
	}

	[[nodiscard]]
	std::expected<Buffer::Data, Error> Buffer::get_data(
		const fastgltf::sources::CustomBuffer&,
		const std::optional<std::filesystem::path>&
	) noexcept
	{
		return Error("Custom buffers are not supported as buffer sources");
	}

	[[nodiscard]]
	std::expected<Buffer::Data, Error> Buffer::get_data(
		const fastgltf::sources::ByteView& view,
		const std::optional<std::filesystem::path>&
	) noexcept
	{
		return view.bytes;
	}

	[[nodiscard]]
	std::expected<Buffer::Data, Error> Buffer::get_data(
		const fastgltf::sources::Fallback&,
		const std::optional<std::filesystem::path>&
	) noexcept
	{
		return Error("Fallback buffer sources are not supported");
	}

	std::expected<Buffer, Error> Buffer::create(
		const fastgltf::Buffer& buffer,
		std::shared_ptr<FileCache> file_cache,
		const std::optional<std::filesystem::path>& directory
	) noexcept
	{
		auto data_result =
			std::visit([&](const auto& source) { return get_data(source, directory); }, buffer.data);
		if (!data_result) return data_result.error().forward("Acquire buffer data failed");

		return Buffer(std::make_unique<std::mutex>(), std::move(file_cache), std::move(*data_result));
	}

	std::expected<std::span<const std::byte>, Error> Buffer::get_span(
		const fastgltf::BufferView& view
	) noexcept
	{
		const std::scoped_lock lock(*mutex);

		if (std::holds_alternative<std::filesystem::path>(data))
		{
			auto mmap_result = file_cache->get(std::get<std::filesystem::path>(data));
			if (!mmap_result) return mmap_result.error().forward("Memory map buffer file failed");
			data = std::move(*mmap_result);
		}

		const auto overload = util::Overload(
			[&](std::span<const std::byte> bytes) -> std::expected<std::span<const std::byte>, Error> {
				if (view.byteOffset + view.byteLength > bytes.size())
					return Error("Byte range out of bounds");

				return bytes.subspan(view.byteOffset, view.byteLength);
			},
			[&](const std::filesystem::path&) -> std::expected<std::span<const std::byte>, Error> {
				UNREACHABLE("Data should never be a path here");
			},
			[&](const std::unique_ptr<std::vector<std::byte>>& vector)
				-> std::expected<std::span<const std::byte>, Error> {
				if (view.byteOffset + view.byteLength > vector->size())
					return Error("Byte range out of bounds");

				return std::span(*vector).subspan(view.byteOffset, view.byteLength);
			},
			[&](const std::shared_ptr<mio::basic_mmap_source<std::byte>>& mmap)
				-> std::expected<std::span<const std::byte>, Error> {
				if (view.byteOffset + view.byteLength > mmap->size())
					return Error("Byte range out of bounds");

				return std::span(mmap->data(), mmap->size()).subspan(view.byteOffset, view.byteLength);
			}
		);

		return std::visit(overload, data);
	}

	std::expected<std::vector<std::byte>, Error> Buffer::get_copy(const fastgltf::BufferView& view) noexcept
	{
		return get_span(view).transform([](std::span<const std::byte> span) {
			return std::vector(std::from_range, span);
		});
	}
}
