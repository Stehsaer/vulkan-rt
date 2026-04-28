#pragma once

#include "vulkan/alloc/buffer.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan
{
	///
	/// @brief Reference to a buffer holding a single element of type `T`
	///
	/// @tparam T Type of the element
	///
	template <typename T>
	class ElementBufferRef
	{
	  public:

		///
		/// @brief Create a new reference from an element buffer
		///
		/// @param buffer Element buffer
		///
		ElementBufferRef(const ElementBuffer<T>& buffer) :
			buffer(buffer)
		{}

		///
		/// @brief Construct a new reference from a raw buffer. Behavior is undefined if the buffer cannot
		/// hold at least one element of type T.
		/// @note When the buffer is larger than `sizeof(T)`, its first `sizeof(T)` bytes will be referenced
		/// as the element
		///
		/// @param buffer
		///
		ElementBufferRef(vk::Buffer buffer) :
			buffer(buffer)
		{}

		ElementBufferRef() = default;

		operator vk::Buffer() const noexcept { return buffer; }
		vk::Buffer operator*() const noexcept { return buffer; }

		///
		/// @brief Size in bytes of the element
		///
		/// @return Size in bytes
		///
		[[nodiscard]]
		size_t size_bytes() const noexcept
		{
			return sizeof(T);
		}

		///
		/// @brief Size as expected by vulkan
		///
		/// @return `sizeof(T)`
		///
		[[nodiscard]]
		size_t size_vk() const noexcept
		{
			return buffer != nullptr ? sizeof(T) : 0;
		}

	  private:

		vk::Buffer buffer = nullptr;

	  public:

		ElementBufferRef(const ElementBufferRef&) = default;
		ElementBufferRef(ElementBufferRef&&) = default;
		ElementBufferRef& operator=(const ElementBufferRef&) = default;
		ElementBufferRef& operator=(ElementBufferRef&&) = default;
	};

	///
	/// @brief Reference to a buffer holding an array of elements of type `T`
	///
	/// @tparam T Type of the elements
	///
	template <typename T>
	class ArrayBufferRef
	{
	  public:

		ArrayBufferRef(const ArrayBuffer<T>& buffer) :
			buffer(buffer),
			element_count(buffer.count())
		{}

		ArrayBufferRef(vk::Buffer buffer, size_t element_count) :
			buffer(buffer),
			element_count(element_count)
		{}

		ArrayBufferRef() = default;

		operator vk::Buffer() const noexcept { return buffer; }
		vk::Buffer operator*() const noexcept { return buffer; }

		///
		/// @brief Size in bytes of the array
		///
		/// @return Size in bytes
		///
		[[nodiscard]]
		size_t size_bytes() const noexcept
		{
			return sizeof(T) * element_count;
		}

		///
		/// @brief Vulkan size of the array
		///
		/// @retval 0 if buffer is invalid
		/// @retval `vk::WholeSize` if buffer is valid but element count is 0
		/// @retval `sizeof(T) * element_count` otherwise
		///
		[[nodiscard]]
		size_t size_vk() const noexcept
		{
			if (buffer == nullptr) return 0;
			if (element_count == 0) return vk::WholeSize;

			return sizeof(T) * element_count;
		}

		///
		/// @brief Get the element count of the array
		///
		/// @return Element count
		///
		[[nodiscard]]
		size_t count() const noexcept
		{
			return element_count;
		}

	  private:

		vk::Buffer buffer = nullptr;
		size_t element_count = 0;

	  public:

		ArrayBufferRef(const ArrayBufferRef&) = default;
		ArrayBufferRef(ArrayBufferRef&&) = default;
		ArrayBufferRef& operator=(const ArrayBufferRef&) = default;
		ArrayBufferRef& operator=(ArrayBufferRef&&) = default;
	};
}
