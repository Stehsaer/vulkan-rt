#pragma once

#include "common/util/error.hpp"

#include <concepts>
#include <functional>
#include <memory>
#include <ranges>
#include <vector>

namespace vulkan::util
{
	///
	/// @brief Helper class for cycling through a list of items, e.g. frame resources
	///
	/// @tparam T Type of the items to cycle through
	///
	template <typename T>
		requires(std::move_constructible<T>)
	class Cycle
	{
		// `back()` for current frame, `front()` for last frame
		std::vector<std::unique_ptr<T>> items;

		Cycle(std::vector<std::unique_ptr<T>> items) noexcept :
			items(std::move(items))
		{}

	  public:

		Cycle(const Cycle&) = delete;
		Cycle(Cycle&&) = default;
		Cycle& operator=(const Cycle&) = delete;
		Cycle& operator=(Cycle&&) = default;

		///
		/// @brief Create a cycle object
		///
		/// @param items Items to cycle through
		/// @return New Cycle instance
		///
		static Cycle<T> create(std::vector<T> items) noexcept
		{
			std::vector<std::unique_ptr<T>> unique_ptr_items;
			for (auto& item : items) unique_ptr_items.push_back(std::make_unique<T>(std::move(item)));
			return Cycle<T>(std::move(unique_ptr_items));
		}

		///
		/// @brief Create a cycle object from a creation function
		///
		/// @param count Number of items to create
		/// @param func Function to create each item
		///
		template <typename F>
			requires(std::is_invocable_r_v<std::expected<T, Error>, F>)
		static std::expected<Cycle<T>, Error> create_by_func(uint32_t count, F func) noexcept
		{
			std::vector<std::unique_ptr<T>> items;
			for (const auto _ : std::views::iota(0u, count))
			{
				std::expected<T, Error> item_expected = std::invoke(func);
				if (!item_expected) return item_expected.error();
				items.push_back(std::make_unique<T>(std::move(*item_expected)));
			}
			return Cycle<T>(std::move(items));
		}

		///
		/// @brief Create a cycle object from a creation function
		///
		/// @param count Number of items to create
		/// @param func Function to create each item
		///
		template <typename F>
			requires(std::is_invocable_r_v<T, F>)
		static Cycle<T> create_by_func(uint32_t count, F func) noexcept
		{
			std::vector<std::unique_ptr<T>> items;
			for (const auto _ : std::views::iota(0u, count))
				items.push_back(std::make_unique<T>(std::invoke(func)));
			return Cycle<T>(std::move(items));
		}

		///
		/// @brief Item for current frame
		///
		/// @return Reference to the current item
		///
		const T& current() const noexcept { return *items.back(); }

		///
		/// @brief Item for previous frame
		///
		/// @return Reference to the previous item
		///
		const T& prev() const noexcept { return *items.front(); }

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