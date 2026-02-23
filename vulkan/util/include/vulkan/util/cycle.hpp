#pragma once

#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>
#include <vector>

namespace vulkan
{
	///
	/// @brief Helper class for cycling through a list of items, e.g. frame resources
	/// @details
	/// #### Creation
	/// Create a `Cycle<T>` by directly supplying an input range into the constructor.
	/// ```cpp
	/// std::vector<vk::raii::Image> images = ...;
	/// auto image_cycle = Cycle(images);
	/// ```
	///
	/// Alternatively, a `Cycle<T>` can also be created in chained style.
	/// ```cpp
	/// auto image_cycle = foo()
	/// 	| Error::collect()
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

		///
		/// @brief Create an empty `Cycle`
		/// @note This is typically used as a placeholder before the actual items are created, e.g. before the
		/// first acquisition of the swapchain images
		///
		Cycle(std::nullopt_t) noexcept {}

		///
		/// @brief Create a `Cycle` with the given items
		///
		/// @param items Items to cycle through
		///
		template <std::ranges::input_range Range>
			requires std::is_constructible_v<T, std::ranges::range_value_t<Range>>
		explicit Cycle(Range items) noexcept
		{
			this->items.reserve(items.size());
			for (auto&& item : items) this->items.emplace_back(std::make_unique<T>(std::move(item)));
		}

		struct Creator
		{
			template <std::ranges::input_range Range>
				requires std::is_constructible_v<T, std::ranges::range_value_t<Range>>
			friend Cycle operator|(Range items, const Creator&) noexcept
			{
				return Cycle(std::forward<Range>(items));
			}
		};

		static constexpr Creator into{};

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

		///
		/// @brief Iterate through the items in pairs of `(previous, current)`
		///
		/// @return Array of pairs of `(previous, current)`
		///
		[[nodiscard]]
		auto iterate_pair() const noexcept
		{
			return std::views::iota(0zu, items.size()) | std::views::transform([this](size_t i) {
					   const auto& current_item = *items[i];
					   const auto& prev_item = *items[(i + 1) % items.size()];
					   return std::make_pair(std::cref(prev_item), std::cref(current_item));
				   });
		}

		///
		/// @brief Iterate through the items
		///
		/// @return Array of const references to the items
		///
		[[nodiscard]]
		auto iterate() const noexcept
		{
			return items | std::views::transform([](const auto& item) -> const T& { return *item; });
		}
	};

	template <std::ranges::input_range Range>
	Cycle(Range&&) -> Cycle<std::remove_cvref_t<std::ranges::range_value_t<Range>>>;
}
