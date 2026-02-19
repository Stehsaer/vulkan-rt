#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>

namespace util
{
	template <size_t... N>
	consteval size_t total_size() noexcept
	{
		return (0 + ... + N);
	}

	template <typename T>
	concept ConcatenableArrayElemType = std::copy_constructible<T> && std::default_initializable<T>;

	///
	/// @brief Concatenates multiple `std::array` into a single one
	///
	/// @tparam T Type of the elements, should be copy constructible and default initializable
	/// @tparam N Sizes of the input arrays
	/// @param arrays The input arrays to concatenate
	/// @return The concatenated array, holding all elements from the input arrays in order
	///
	template <ConcatenableArrayElemType T, size_t... N>
	constexpr std::array<T, total_size<N...>()> array_concat(const std::array<T, N>&... arrays) noexcept
	{
		std::array<T, total_size<N...>()> result;

		size_t offset = 0;
		((std::ranges::copy(arrays, std::next(result.begin(), offset)), offset += N), ...);

		return result;
	}
}
