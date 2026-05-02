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
		return [&func](auto&& tuple) {
			return std::apply(std::forward<decltype(func)>(func), std::forward<decltype(tuple)>(tuple));
		};
	}
}
