#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace util
{
	template <size_t... N>
	consteval size_t total_size() noexcept
	{
		return (0 + ... + N);
	}

	namespace impl
	{
		template <typename T, size_t N>
		constexpr auto as_array(const std::array<T, N>& array) noexcept
		{
			return array;
		}

		template <typename T>
		constexpr auto as_array(const T& value) noexcept
		{
			return std::to_array<T>({value});
		}

		template <typename T>
			requires std::is_array_v<T>
		constexpr auto as_array(const T& c_array) noexcept
		{
			return std::to_array(c_array);
		}

		template <typename T>
		concept ConcatenableArrayElemType = std::copy_constructible<T> && std::default_initializable<T>;

		template <ConcatenableArrayElemType T, size_t... N>
		constexpr std::array<T, total_size<N...>()> array_concat(const std::array<T, N>&... arrays) noexcept
		{
			std::array<T, total_size<N...>()> result;

			size_t offset = 0;
			((std::ranges::copy(arrays, std::next(result.begin(), offset)), offset += N), ...);

			return result;
		}
	}

	///
	/// @brief Concatenate multiple objects or array of objects into a single `std::array`.
	///
	/// @param arrays Objects or arrays to concatenate. Each argument can be a single object, a `std::array`,
	/// or a C-style array. All elements must be of the same type and must be copy constructible and default
	/// initializable.
	/// @return A `std::array` containing all elements from the input arrays.
	///
	template <typename... T>
	constexpr auto array_concat(const T&... arrays) noexcept
	{
		return impl::array_concat(impl::as_array(arrays)...);
	}

	template <typename F>
	struct MapArrayFunctor
	{
		F func;

		template <typename T, size_t N>
			requires std::invocable<F, T>
		friend constexpr auto operator|(const std::array<T, N>& array, MapArrayFunctor<F> functor)
		{
			using ResultType = std::remove_cvref_t<std::invoke_result_t<F, T>>;
			std::array<ResultType, N> result;
			for (size_t i = 0; i < N; ++i) result[i] = std::invoke(functor.func, array[i]);
			return result;
		}
	};

	///
	/// @brief Map an array into another array
	///
	/// @tparam F Mapping function type
	/// @param func Mapping function
	/// @return Mapped array
	///
	template <typename F>
	constexpr auto map_array(F func)
	{
		return MapArrayFunctor<F>{.func = func};
	}

	struct ArrayToTupleFunctor
	{
		template <typename T, size_t N>
		friend constexpr auto operator|(const std::array<T, N>& array, ArrayToTupleFunctor)
		{
			return [&array]<size_t... I>(std::index_sequence<I...>) {
				return std::make_tuple(array[I]...);
			}(std::make_index_sequence<N>());
		}
	};

	///
	/// @brief Utility functor converting an array to tuple
	///
	static constexpr ArrayToTupleFunctor array_to_tuple = {};
}
