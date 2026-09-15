#pragma once

#include <cstddef>
#include <libassert/assert.hpp>
#include <memory_resource>
#include <utility>

namespace util
{
	///
	/// @brief Unique pointer allocated on PMR allocators
	///
	/// @tparam T Type of the object
	///
	template <typename T>
		requires(!std::is_array_v<T>)
	class PmrUniquePtr
	{
	  public:

		/*===== Constructor =====*/

		///
		/// @brief Create an empty unique pointer
		///
		PmrUniquePtr() = default;

		///
		/// @brief Create an empty unique pointer
		///
		PmrUniquePtr(std::nullptr_t) :
			PmrUniquePtr() {};

		///
		/// @brief Create a unique pointer and construct the object inplace using given arguments
		///
		/// @tparam Args Type of arguments
		/// @param resource PMR resource
		/// @param args Arguments to construct the object
		/// @return New unique pointer
		///
		template <typename... Args>
		static PmrUniquePtr from(std::pmr::memory_resource& resource, Args&&... args)
		{
			std::pmr::polymorphic_allocator<T> allocator(&resource);
			auto* data_ptr = allocator.template new_object<T>(std::forward<Args>(args)...);
			return PmrUniquePtr(&resource, data_ptr);
		}

		/*===== Mutate =====*/

		///
		/// @brief Release the object and reset the unique pointer
		///
		void reset() noexcept
		{
			if (resource != nullptr && data_ptr != nullptr)
				std::pmr::polymorphic_allocator<T>(resource).delete_object(std::exchange(data_ptr, nullptr));
		}

		///
		/// @brief Swap two unique pointers
		///
		/// @param other Other unique pointer
		///
		void swap(PmrUniquePtr& other) noexcept
		{
			std::swap(resource, other.resource);
			std::swap(data_ptr, other.data_ptr);
		}

		/*===== Access =====*/

		operator bool() const noexcept { return data_ptr != nullptr; }

		bool operator==(std::nullptr_t) const noexcept { return data_ptr == nullptr; }

		///
		/// @brief Get the underlying pointer to the object
		///
		/// @return Pointer to the object, will be `nullptr` if the unique pointer is empty
		///
		T* get() const noexcept { return data_ptr; }

		T& operator*() const noexcept
		{
			DEBUG_ASSERT(data_ptr != nullptr);
			return *data_ptr;
		}

		T* operator->() const noexcept
		{
			DEBUG_ASSERT(data_ptr != nullptr);
			return data_ptr;
		}

	  private:

		std::pmr::memory_resource* resource = nullptr;
		T* data_ptr = nullptr;

		PmrUniquePtr(std::pmr::memory_resource* resource, T* data_ptr) :
			resource(resource),
			data_ptr(data_ptr)
		{}

	  public:

		PmrUniquePtr(const PmrUniquePtr&) = delete;
		PmrUniquePtr& operator=(const PmrUniquePtr&) = delete;

		PmrUniquePtr(PmrUniquePtr&& src) noexcept :
			resource(std::exchange(src.resource, nullptr)),
			data_ptr(std::exchange(src.data_ptr, nullptr))
		{}

		PmrUniquePtr& operator=(PmrUniquePtr&& other) noexcept
		{
			if (this != &other) [[likely]]
			{
				if (data_ptr != nullptr) std::pmr::polymorphic_allocator<T>(resource).delete_object(data_ptr);
				resource = std::exchange(other.resource, nullptr);
				data_ptr = std::exchange(other.data_ptr, nullptr);
			}

			return *this;
		}

		~PmrUniquePtr() noexcept { reset(); }
	};
}

namespace std
{
	template <typename T>
	void swap(util::PmrUniquePtr<T>& lhs, util::PmrUniquePtr<T>& rhs) noexcept
	{
		lhs.swap(rhs);
	}
}
