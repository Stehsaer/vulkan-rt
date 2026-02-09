#pragma once

#include <concepts>
#include <memory>
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