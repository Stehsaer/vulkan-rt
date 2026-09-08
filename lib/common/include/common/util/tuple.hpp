#pragma once

#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>

namespace util
{
	///
	/// @brief Create a new tuple mapped by a given function
	///
	/// @tparam Func Type of the function
	/// @tparam Elem Type of the elements held in the tuple
	///
	/// @param input Input tuple
	/// @param f Transformation function
	///
	/// @return Mapped tuple
	///
	template <typename Func, typename... Elem>
		requires(std::invocable<Func, const Elem&> && ...)
	constexpr std::tuple<std::invoke_result_t<Func, Elem>...> map_tuple(
		const std::tuple<Elem...>& input,
		Func f
	) noexcept((std::is_nothrow_invocable_v<Func, Elem> && ...))
	{
		return std::apply(
			[&](const Elem&... elems) { return std::make_tuple(std::invoke(f, elems)...); },
			input
		);
	}

	///
	/// @brief Fold left each element with an initial value
	///
	/// @tparam T Type of the tuple
	/// @tparam Func Type of the folding function
	/// @tparam Elem Type of the elements held in the tuple
	///
	/// @param input Input tuple
	/// @param init Initial value
	/// @param f Folding function
	///
	/// @return Folded value
	///
	template <typename T, std::invocable<T, T> Func, std::convertible_to<T>... Elem>
		requires(std::convertible_to<std::invoke_result_t<Func, T, T>, T> && std::is_move_assignable_v<T>)
	constexpr T fold_left_tuple(const std::tuple<Elem...>& input, T init, Func f) noexcept(
		std::is_nothrow_invocable_v<Func, T, T>
	)
	{
		std::apply(
			[&](auto... elems) { ((init = std::invoke(f, init, static_cast<T>(elems))), ...); },
			input
		);
		return init;
	}
}
