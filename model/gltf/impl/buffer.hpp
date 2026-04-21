#pragma once

#include "common/util/error.hpp"
#include "file-cache.hpp"

#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <mio/mmap.hpp>
#include <mutex>

namespace model::gltf::impl
{
	// Buffer with deferred loading
	class Buffer
	{
	  public:

		// Get a copy of the buffer data for the given buffer view
		[[nodiscard]]
		std::expected<std::vector<std::byte>, Error> get_copy(const fastgltf::BufferView& view) noexcept;

		// Get a span of the buffer data for the given buffer view.
		// The span stays valid throughout lifetime of buffer
		[[nodiscard]]
		std::expected<std::span<const std::byte>, Error> get_span(const fastgltf::BufferView& view) noexcept;

		[[nodiscard]]
		static std::expected<Buffer, Error> create(
			const fastgltf::Buffer& buffer,
			std::shared_ptr<FileCache> file_cache,
			const std::optional<std::filesystem::path>& directory = std::nullopt
		) noexcept;

	  private:

		using Data = std::variant<
			std::span<const std::byte>,
			std::filesystem::path,
			std::unique_ptr<std::vector<std::byte>>,
			std::shared_ptr<mio::basic_mmap_source<std::byte>>
		>;

		std::unique_ptr<std::mutex> mutex;
		std::shared_ptr<FileCache> file_cache;
		Data data;

		explicit Buffer(std::unique_ptr<std::mutex> mutex, std::shared_ptr<FileCache> file_cache, Data data) :
			mutex(std::move(mutex)),
			file_cache(std::move(file_cache)),
			data(std::move(data))
		{}

		[[nodiscard]]
		static std::expected<Data, Error> get_data(
			const std::monostate&,
			const std::optional<std::filesystem::path>& directory
		) noexcept;

		[[nodiscard]]
		static std::expected<Data, Error> get_data(
			const fastgltf::sources::BufferView&,
			const std::optional<std::filesystem::path>& directory
		) noexcept;

		[[nodiscard]]
		static std::expected<Data, Error> get_data(
			const fastgltf::sources::URI&,
			const std::optional<std::filesystem::path>& directory
		) noexcept;

		[[nodiscard]]
		static std::expected<Data, Error> get_data(
			const fastgltf::sources::Array&,
			const std::optional<std::filesystem::path>& directory
		) noexcept;

		[[nodiscard]]
		static std::expected<Data, Error> get_data(
			const fastgltf::sources::Vector&,
			const std::optional<std::filesystem::path>& directory
		) noexcept;

		[[nodiscard]]
		static std::expected<Data, Error> get_data(
			const fastgltf::sources::CustomBuffer&,
			const std::optional<std::filesystem::path>& directory
		) noexcept;

		[[nodiscard]]
		static std::expected<Data, Error> get_data(
			const fastgltf::sources::ByteView&,
			const std::optional<std::filesystem::path>& directory
		) noexcept;

		[[nodiscard]]
		static std::expected<Data, Error> get_data(
			const fastgltf::sources::Fallback&,
			const std::optional<std::filesystem::path>& directory
		) noexcept;

	  public:

		Buffer(const Buffer&) = delete;
		Buffer(Buffer&&) = default;
		Buffer& operator=(const Buffer&) = delete;
		Buffer& operator=(Buffer&&) = default;
	};
}
