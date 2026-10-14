#pragma once

#include <algorithm>
#include <iterator>
#include <set>

namespace util::set_operator
{
	///
	/// @brief Removes element from `a` that exists in `b`
	///
	/// @param a Set a
	/// @param b Set b
	/// @return Result of removal
	///
	template <typename T>
	[[nodiscard]]
	std::set<T> operator-(const std::set<T>& a, const std::set<T>& b) noexcept
	{
		std::set<T> result;
		std::ranges::set_difference(a, b, std::inserter(result, result.end()));
		return result;
	}

	///
	/// @brief Calculates union of two sets
	///
	/// @param a Set a
	/// @param b Set b
	/// @return Union
	///
	template <typename T>
	[[nodiscard]]
	std::set<T> operator+(const std::set<T>& a, const std::set<T>& b) noexcept
	{
		std::set<T> result = a;
		result.insert_range(b);
		return result;
	}
}
