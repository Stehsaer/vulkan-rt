#pragma once

#include <concepts>
#include <memory>
#include <vector>

namespace vulkan
{
	///
	/// @brief Helper class for cycling through a list of items, e.g. frame resources
	/// @details
	/// #### Creation
	/// Create a `Cycle<T>` by directly supplying a `std::vector<T>` into the constructor.
	/// ```cpp
	/// std::vector<vk::raii::Image> images = ...;
	/// auto image_cycle = Cycle(images);
	/// ```
	///
	/// Alternatively, a `Cycle<T>` can also be created in chained style.
	/// ```cpp
	/// auto image_cycle = foo()
	/// 	| Error::collect_vec()
	/// 	| Error::unwrap()
	/// 	| Cycle::into;
	/// ```
	///
	/// #### Usage
	/// - Call @p cycle to cycle the elements by 1 step. Typically called at the start of a new frame.
	/// - Use @p current and @p prev to access the current item and the previous item.
	///
	/// @tparam T Type of the items to cycle through
	///
	template <typename T>
		requires(std::move_constructible<T>)
	class Cycle
	{
		// `back()` for current frame, `front()` for last frame
		std::vector<std::unique_ptr<T>> items;

	  public:

		Cycle(const Cycle&) = delete;
		Cycle(Cycle&&) = default;
		Cycle& operator=(const Cycle&) = delete;
		Cycle& operator=(Cycle&&) = default;

		Cycle(std::vector<T> items) noexcept
		{
			items.reserve(items.size());
			for (auto& item : items) this->items.emplace_back(std::make_unique<T>(std::move(item)));
		}

		struct Creator
		{
			friend Cycle operator|(std::vector<T> items, const Creator&) noexcept
			{
				return Cycle(std::move(items));
			}
		};

		static constexpr Creator into{};

		///
		/// @brief Create a cycle object
		///
		/// @param items Items to cycle through
		/// @return New Cycle instance
		///
		[[nodiscard]]
		static Cycle<T> create(std::vector<T> items) noexcept
		{
			std::vector<std::unique_ptr<T>> unique_ptr_items;
			for (auto& item : items) unique_ptr_items.push_back(std::make_unique<T>(std::move(item)));
			return Cycle<T>(std::move(unique_ptr_items));
		}

		///
		/// @brief Item for current frame
		///
		/// @return Reference to the current item
		///
		[[nodiscard]]
		const T& current() const noexcept
		{
			return *items.back();
		}

		///
		/// @brief Item for previous frame
		///
		/// @return Reference to the previous item
		///
		[[nodiscard]]
		const T& prev() const noexcept
		{
			return *items.front();
		}

		///
		/// @brief Cycle to the next item, often called at the end of a frames
		///
		///
		void cycle() noexcept
		{
			auto item = std::move(items.back());
			items.pop_back();
			items.insert(items.begin(), std::move(item));
		}
	};
}
