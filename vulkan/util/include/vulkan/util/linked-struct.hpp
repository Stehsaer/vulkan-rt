#pragma once

#include <any>
#include <concepts>
#include <memory>
#include <type_traits>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vulkan
{
	template <typename T>
	concept LinkableType =
		requires(T a) {
			a.pNext;
			a.sType;
		}
		&& std::copy_constructible<T>
		&& !std::is_pointer_v<T>
		&& !std::is_reference_v<T>
		&& !std::is_const_v<T>;

	///
	/// @brief Helper for dynamically linking Vulkan structures
	/// @details
	/// To use this helper, first create a instance using a primary structure (i.e the first element of the
	/// chain):
	/// ```cpp
	/// auto linked = LinkedStruct(primary_blah_blah);
	/// ```
	///
	/// Then, chain the struct by calling @p push
	/// ```cpp
	/// linked.push(blah_blah).push(blah_blah);
	/// ```
	///
	/// If needed, pop the last linked struct by calling @p try_pop
	/// ```cpp
	/// auto pop_result = linked.try_pop();
	/// if (!pop_result) ...
	/// ```
	///
	/// @tparam Primary Primary structure type, must have `pNext` member
	///
	template <LinkableType Primary>
	class LinkedStruct
	{
		std::unique_ptr<Primary> primary;
		std::vector<std::any> linked_structs;
		std::vector<std::variant<void**, const void**>> pnext_ptrs;

	  public:

		LinkedStruct(const LinkedStruct&) = delete;
		LinkedStruct(LinkedStruct&&) = default;
		LinkedStruct& operator=(const LinkedStruct&) = delete;
		LinkedStruct& operator=(LinkedStruct&&) = default;

		///
		/// @brief Construct a new struct linker by providing the primary structure
		///
		/// @param primary_struct Primary structure
		///
		LinkedStruct(const Primary& primary_struct) noexcept :
			primary(std::make_unique<Primary>(primary_struct))
		{
			pnext_ptrs.push_back(&this->primary->pNext);
		}

		///
		/// @brief Push a new structure to the link chain
		///
		/// @tparam T Structure type, must have `pNext` member
		/// @param new_struct Structure to link
		/// @return Reference to this linker
		///
		template <LinkableType T>
		LinkedStruct& push(const T& new_struct) noexcept
		{
			std::shared_ptr<T> struct_ptr = std::make_shared<T>(new_struct);

			std::visit([&](auto&& ptr) { *ptr = struct_ptr.get(); }, pnext_ptrs.back());
			pnext_ptrs.push_back(&struct_ptr->pNext);

			linked_structs.push_back(std::make_any<std::shared_ptr<T>>(struct_ptr));

			return *this;
		}

		///
		/// @brief Pop the last linked structure from the chain
		///
		/// @retval true A structure was popped
		/// @retval false No structure to pop
		///
		[[nodiscard]]
		bool try_pop() noexcept
		{
			if (linked_structs.empty()) return false;
			linked_structs.pop_back();

			pnext_ptrs.pop_back();
			std::visit([&](auto&& ptr) { *ptr = nullptr; }, pnext_ptrs.back());

			return true;
		}

		///
		/// @brief Get the primary structure
		///
		/// @return Const reference to the primary structure
		///
		[[nodiscard]]
		const Primary& get() const noexcept
		{
			return *primary;
		}
	};
}