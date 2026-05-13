#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vulkan
{
	template <typename T>
	concept VulkanPNextType = std::same_as<std::remove_reference_t<T>, const void*>
		|| std::same_as<std::remove_reference_t<T>, void*>;

	template <typename T>
	concept LinkableType =
		requires(T a) {
			{ a.pNext } -> VulkanPNextType;
			{ a.sType } -> std::convertible_to<vk::StructureType>;
		}
		&& std::copy_constructible<T>
		&& !std::is_pointer_v<T>
		&& !std::is_reference_v<T>
		&& !std::is_const_v<T>;

	namespace impl
	{
		struct LinkedNodeBase
		{
			virtual ~LinkedNodeBase() = default;

			virtual void set_pnext(void* pnext) noexcept = 0;
			virtual void* get_void_ptr() noexcept = 0;
		};

		template <LinkableType T>
		class LinkedNode : public LinkedNodeBase
		{
		  public:

			LinkedNode(const T& val) noexcept :
				value(val)
			{}

			void set_pnext(void* pnext) noexcept override { value.pNext = pnext; }

			[[nodiscard]]
			void* get_void_ptr() noexcept override
			{
				return &value;
			}

			[[nodiscard]]
			const T& get() const noexcept
			{
				return value;
			}

		  private:

			T value;

		  public:

			LinkedNode(const LinkedNode&) = delete;
			LinkedNode(LinkedNode&&) = delete;
			LinkedNode& operator=(const LinkedNode&) = delete;
			LinkedNode& operator=(LinkedNode&&) = delete;
		};
	}

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
		std::unique_ptr<impl::LinkedNode<Primary>> primary;
		std::vector<std::unique_ptr<impl::LinkedNodeBase>> secondary_nodes;

	  public:

		LinkedStruct(const LinkedStruct&) = delete;
		LinkedStruct(LinkedStruct&&) = default;
		LinkedStruct& operator=(const LinkedStruct&) = delete;
		LinkedStruct& operator=(LinkedStruct&&) = default;

		///
		/// @brief Construct a new linked struct by providing the primary struct
		///
		/// @param primary_struct Primary struct
		///
		LinkedStruct(const Primary& primary_struct) noexcept :
			primary(std::make_unique<impl::LinkedNode<Primary>>(primary_struct))
		{}

		///
		/// @brief Construct a new linked struct by providing the primary and secondary structs
		///
		/// @tparam T Types of secondary structs
		/// @param primary_struct Primary struct
		/// @param secondary_structs Secondary structs
		///
		template <LinkableType... T>
		LinkedStruct(const Primary& primary_struct, const T&... secondary_structs) :
			LinkedStruct(primary_struct)
		{
			(push(secondary_structs), ...);
		}

		///
		/// @brief Push a new structure to the link chain
		///
		/// @tparam T Structure type, must have `pNext` member
		/// @param new_struct Structure to link
		/// @return Reference to this linker
		///
		template <LinkableType T>
		auto push(this auto&& self, const T& new_struct) noexcept -> decltype(self)
		{
			impl::LinkedNodeBase& prev_node =
				self.secondary_nodes.empty() ? *self.primary : *self.secondary_nodes.back();

			self.secondary_nodes.push_back(std::make_unique<impl::LinkedNode<T>>(new_struct));
			impl::LinkedNodeBase& node = *self.secondary_nodes.back();

			prev_node.set_pnext(node.get_void_ptr());
			node.set_pnext(nullptr);

			return std::forward<decltype(self)>(self);
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
			if (secondary_nodes.empty()) return false;
			secondary_nodes.pop_back();

			impl::LinkedNodeBase& prev_node = secondary_nodes.empty() ? *primary : *secondary_nodes.back();
			prev_node.set_pnext(nullptr);

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
			return primary->get();
		}
	};
}
