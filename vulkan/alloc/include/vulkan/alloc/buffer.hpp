#pragma once

#include "common/util/error.hpp"
#include "common/util/span.hpp"
#include "vulkan/alloc/wrapper.hpp"

#include <expected>
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan
{
	class Allocator;

	///
	/// @brief Allocated buffer, frees the memory on deconstruction
	/// @warning For type safety, consider using `ElementBuffer` and `ArrayBuffer` instead of this class
	/// directly. This class is more suitable for low-level usage where the buffer is used as a raw byte
	/// buffer and the type is not known at compile time.
	///
	/// @details
	/// - Can be directly used as if it were a `vk::Buffer`
	/// - For host-visible buffers, use `upload` and `download` to transfer data between host and the buffer.
	/// See the docs of these two functions for more detail
	///
	class Buffer
	{
	  public:

		operator vk::Buffer() const noexcept { return wrapper->buffer; }
		vk::Buffer operator*() const noexcept { return wrapper->buffer; }

		///
		/// @brief Upload data to buffer
		/// @warning This function should be called if and only if the buffer is host-visible.
		///
		/// @param data Host data
		/// @param dst_offset Destination offset in buffer
		/// @return Void or Error
		///
		[[nodiscard]]
		std::expected<void, Error> upload(
			std::span<const std::byte> data,
			size_t dst_offset = 0
		) const noexcept;

		///
		/// @brief Download data from buffer to host
		/// @warning This function should be called if and only if the buffer is host-visible.
		///
		/// @param data Data span to fill
		/// @param src_offset Source offset in buffer
		/// @return Void or Error
		///
		[[nodiscard]]
		std::expected<void, Error> download(std::span<std::byte> data, size_t src_offset = 0) const noexcept;

	  private:

		std::unique_ptr<impl::BufferWrapper> wrapper;

		explicit Buffer(std::unique_ptr<impl::BufferWrapper> wrapper) :
			wrapper(std::move(wrapper))
		{}

		friend class ::vulkan::Allocator;

	  public:

		Buffer(const Buffer&) = delete;
		Buffer(Buffer&&) = default;
		Buffer& operator=(const Buffer&) = delete;
		Buffer& operator=(Buffer&&) = default;
	};

	///
	/// @brief Type-safe buffer that holds a single element of type T
	///
	/// @tparam T Type of the element
	///
	template <typename T>
	class ElementBuffer
	{
	  public:

		///
		/// @brief Explicitly construct a new element buffer from a raw buffer
		/// @warning The behavior is undefined if the buffer cannot hold at least one element of type T.
		///
		/// @param buffer Raw buffer to construct the element buffer from
		///
		explicit ElementBuffer(Buffer buffer) :
			buffer(std::move(buffer))
		{}

		operator vk::Buffer() const noexcept { return buffer; }
		vk::Buffer operator*() const noexcept { return buffer; }

		///
		/// @brief Upload an element to the buffer
		///
		/// @param element Reference to the element to upload
		///
		[[nodiscard]]
		std::expected<void, Error> upload(const T& element) const noexcept
		{
			return buffer.upload(util::object_as_bytes(element));
		}

		///
		/// @brief Download an element from the buffer
		///
		/// @param element Reference to the element to fill with downloaded data
		///
		[[nodiscard]]
		std::expected<void, Error> download(T& element) const noexcept
		{
			return buffer.download(util::object_as_bytes(element));
		}

	  private:

		Buffer buffer;

	  public:

		ElementBuffer(const ElementBuffer&) = delete;
		ElementBuffer(ElementBuffer&&) = default;
		ElementBuffer& operator=(const ElementBuffer&) = delete;
		ElementBuffer& operator=(ElementBuffer&&) = default;
	};

	///
	/// @brief Type-safe buffer that holds an array of elements of type T.
	///
	/// @tparam T Type of the elements
	///
	template <typename T>
	class ArrayBuffer
	{
	  public:

		///
		/// @brief Explicitly construct a new array buffer from a raw buffer and element count
		/// @warning The behavior is undefined if the buffer cannot hold at least `element_count` elements of
		/// type T.
		///
		/// @param buffer Raw buffer to construct the array buffer from
		/// @param element_count Number of elements the buffer can hold
		///
		explicit ArrayBuffer(Buffer buffer, size_t element_count) :
			buffer(std::move(buffer)),
			element_count(element_count)
		{}

		operator vk::Buffer() const noexcept { return buffer; }
		vk::Buffer operator*() const noexcept { return buffer; }

		///
		/// @brief Upload an array of elements to the buffer
		///
		/// @param elements Span of elements to upload
		/// @param dst_offset Destination offset in number of elements (not bytes)
		///
		[[nodiscard]]
		std::expected<void, Error> upload(std::span<const T> elements, size_t dst_offset = 0) const noexcept
		{
			ASSUME(dst_offset + elements.size() <= element_count);
			return buffer.upload(std::as_bytes(elements), dst_offset * sizeof(T));
		}

		///
		/// @brief Download an array of elements from the buffer to host
		///
		/// @param elements Span of elements to fill with downloaded data
		/// @param src_offset Source offset in number of elements (not bytes)
		///
		[[nodiscard]]
		std::expected<void, Error> download(std::span<T> elements, size_t src_offset = 0) const noexcept
		{
			ASSUME(src_offset + elements.size() <= element_count);
			return buffer.download(std::as_bytes(elements), src_offset * sizeof(T));
		}

		///
		/// @brief Element count that the buffer holds
		///
		/// @return Element count
		///
		[[nodiscard]]
		uint32_t count() const noexcept
		{
			return element_count;
		}

	  private:

		Buffer buffer;
		size_t element_count;

	  public:

		ArrayBuffer(const ArrayBuffer&) = delete;
		ArrayBuffer(ArrayBuffer&&) = default;
		ArrayBuffer& operator=(const ArrayBuffer&) = delete;
		ArrayBuffer& operator=(ArrayBuffer&&) = default;
	};
}
