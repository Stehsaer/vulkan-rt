#pragma once

#include <tuple>
#include <utility>

namespace util
{
	constexpr class TupleArgsTag
	{
	} tuple_args;

	template <typename F>
	constexpr auto operator|(F&& func, TupleArgsTag)
	{
		return [func = std::forward<F>(func)](auto&& tuple) {
			return std::apply(func, std::forward<decltype(tuple)>(tuple));
		};
	}
}
