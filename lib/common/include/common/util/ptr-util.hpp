#pragma once

#include <functional>
#include <optional>

namespace util
{
	template <typename T>
		requires(!std::is_const_v<T>)
	std::optional<std::reference_wrapper<T>> to_optional_ref(T* ptr) noexcept
	{
		if (ptr)
			return std::ref(*ptr);
		else
			return std::nullopt;
	}

	template <typename T>
		requires(!std::is_const_v<T>)
	std::optional<std::reference_wrapper<const T>> to_optional_ref(const T* ptr) noexcept
	{
		if (ptr)
			return std::cref(*ptr);
		else
			return std::nullopt;
	}
}