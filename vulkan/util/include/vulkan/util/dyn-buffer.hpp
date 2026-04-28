#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/common-context.hpp"

#include <cstddef>
#include <vulkan/vulkan_handles.hpp>

namespace vulkan
{
	///
	/// @brief Dynamic sized GPU-side buffer
	/// @details Dynamically resizable GPU-side buffer, with built-in delayed shrinking algorithm
	///
	/// @note For type-safety, consider using `DynArrayBuffer` instead of this class directly
	///
	/// @warning
	/// - Data in the buffer is not guaranteed to be preserved after resize. Always consider the data
	/// as invalid or cleared after resize.
	/// - All calls are **NOT** thread-safe. Caller should synchronize calls if needed.
	///
	class DynBuffer
	{
	  public:

		// Minimum size of the buffer
		static constexpr size_t MIN_SIZE = 3072;

		///
		/// @brief Resize the buffer to at least new_size. Size will be checked in this call and will shrink
		/// if needed
		///
		/// @warning
		/// - Data in the buffer is not guaranteed to be preserved after resize. Always consider the
		/// data as invalid or cleared after resize.
		/// - **NOT** thread-safe. Caller should synchronize calls if needed.
		/// - After a failing resize, the buffer will be in an invalid state and should not be used until the
		/// next successful resize
		///
		/// @param context Device context
		/// @param new_size Requested size in bytes
		/// @return Success, or error if resize failed
		///
		[[nodiscard]]
		std::expected<void, Error> resize(const vulkan::DeviceContext& context, size_t new_size) noexcept;

		operator const Buffer&() const noexcept { return buffer.value(); }
		operator vk::Buffer() const noexcept { return buffer.value(); }
		const Buffer& operator*() const noexcept { return buffer.value(); }
		const Buffer* operator->() const noexcept { return &buffer.value(); }

		///
		/// @brief Get the size of the buffer in bytes. Will return 0 if no buffer is allocated
		/// @warning **NOT** thread-safe. Caller should synchronize calls if needed.
		///
		/// @return Size of the buffer in bytes
		///
		[[nodiscard]]
		size_t size_bytes() const noexcept
		{
			return size;
		}

		///
		/// @brief Get the actual size (capacity) of the allocated buffer in bytes. Will return 0 if no buffer
		/// is allocated
		/// @warning **NOT** thread-safe. Caller should synchronize calls if needed.
		///
		/// @return Capacity of the allocated buffer in bytes
		///
		[[nodiscard]]
		size_t capacity_bytes() const noexcept
		{
			return capacity;
		}

		///
		/// @brief Query if the buffer is currently allocated and valid
		/// @warning **NOT** thread-safe. Caller should synchronize calls if needed.
		///
		/// @return `true` if allocated and valid
		///
		[[nodiscard]]
		bool valid() const noexcept
		{
			return buffer.has_value();
		}

		///
		/// @brief Create a dynamic buffer with the given usage flags. Not allocated until first call to
		/// `resize()`
		///
		/// @param usage Buffer usage flags
		/// @param memory_usage Memory usage type
		///
		explicit DynBuffer(vk::BufferUsageFlags usage, vulkan::MemoryUsage memory_usage) noexcept :
			usage(usage),
			memory_usage(memory_usage)
		{}

	  private:

		// Counter threshold to trigger shrinking
		static constexpr size_t SHRINK_COUNTER_THRES = 32;

		// Counter decrement delta when not oversized
		static constexpr size_t FIT_DELTA = 3;

		// Maximum size of the buffer
		static constexpr size_t MAX_SIZE = 1zu << 4 * 10;  // 1 TB

		size_t capacity = 0;
		size_t size = 0;
		size_t shrink_counter = 0;
		vk::BufferUsageFlags usage;
		vulkan::MemoryUsage memory_usage;
		std::optional<Buffer> buffer = std::nullopt;

	  public:

		DynBuffer(const DynBuffer&) = delete;
		DynBuffer(DynBuffer&&) = default;
		DynBuffer& operator=(const DynBuffer&) = delete;
		DynBuffer& operator=(DynBuffer&&) = default;
	};

	///
	/// @brief Typed version of `DynBuffer` for structured buffers. Item count is tracked and resize is
	/// based on item count instead of byte size.
	///
	/// @tparam T Type of the items to be stored in the buffer. Must be trivially copyable.
	///
	template <typename T>
		requires std::is_trivially_copyable_v<T>
	class DynArrayBuffer
	{
	  public:

		///
		/// @brief Resize the buffer to hold at least new_size_items items. Size will be checked in this call
		/// and will shrink if needed
		///
		/// @warning
		/// - Data in the buffer is not guaranteed to be preserved after resize. Always consider the
		/// data as invalid or cleared after resize.
		/// - **NOT** thread-safe. Caller should synchronize calls if needed.
		/// - After a failing resize, the buffer will be in an invalid state and should not be used until the
		/// next successful resize
		///
		/// @param context Device context
		/// @param new_size_items Requested item count
		/// @return Success, or error if resize failed
		///
		[[nodiscard]]
		std::expected<void, Error> resize(
			const vulkan::DeviceContext& context,
			size_t new_size_items
		) noexcept
		{
			auto result = buffer.resize(context, new_size_items * sizeof(T));
			if (!result) return result.error().forward("Resize dynamic structured buffer failed");

			item_count = new_size_items;
			return {};
		}

		operator ArrayBufferRef<T>() const noexcept { return ArrayBufferRef<T>(*buffer, item_count); }
		operator const Buffer&() const noexcept { return *buffer; }
		operator vk::Buffer() const noexcept { return buffer; }
		const Buffer& operator*() const noexcept { return *buffer; }
		const DynBuffer& operator->() const noexcept { return buffer; }

		///
		/// @brief Get reference to the array buffer
		///
		/// @return Reference to the array buffer
		///
		ArrayBufferRef<T> ref() const noexcept { return *this; }

		///
		/// @brief Get item count that the buffer holds.
		/// @warning **NOT** thread-safe. Caller should synchronize calls if needed.
		///
		/// @return Item count
		///
		[[nodiscard]]
		size_t count() const noexcept
		{
			return item_count;
		}

		///
		/// @brief Get the size of the buffer in bytes. Will return 0 if no buffer is allocated
		/// @warning **NOT** thread-safe. Caller should synchronize calls if needed.
		///
		/// @return Size of the buffer in bytes
		///
		[[nodiscard]]
		size_t size_bytes() const noexcept
		{
			return buffer.size_bytes();
		}

		///
		/// @brief Get capacity of the buffer in bytes. Will return 0 if no buffer is allocated.
		/// @warning **NOT** thread-safe. Caller should synchronize calls if needed.
		///
		/// @return Capacity of the buffer in bytes
		///
		[[nodiscard]]
		size_t capacity_bytes() const noexcept
		{
			return buffer.capacity_bytes();
		}

		///
		/// @brief Check if the buffer is currently allocated and valid
		/// @warning **NOT** thread-safe. Caller should synchronize calls if needed.
		///
		/// @return `true` if allocated and valid
		///
		[[nodiscard]]
		bool valid() const noexcept
		{
			return buffer.valid();
		}

		///
		/// @brief Construct a new dynamic structured buffer with the given usage flags. Not allocated until
		/// first call to `resize()`
		///
		/// @param usage Buffer usage flags
		/// @param memory_usage Memory usage type
		///
		explicit DynArrayBuffer(vk::BufferUsageFlags usage, vulkan::MemoryUsage memory_usage) noexcept :
			buffer(usage, memory_usage)
		{}

	  private:

		DynBuffer buffer;
		size_t item_count = 0;

	  public:

		DynArrayBuffer(const DynArrayBuffer&) = delete;
		DynArrayBuffer(DynArrayBuffer&&) = default;
		DynArrayBuffer& operator=(const DynArrayBuffer&) = delete;
		DynArrayBuffer& operator=(DynArrayBuffer&&) = default;
	};
}
