#pragma once

#include <memory>

namespace util
{
	template <typename T>
		requires std::move_constructible<T>
	std::shared_ptr<T> to_shared_ptr(T&& value) noexcept
	{
		return std::make_shared<T>(std::forward<T>(value));
	}

	template <typename T>
		requires std::move_constructible<T>
	std::unique_ptr<T> to_unique_ptr(T&& value) noexcept
	{
		return std::make_unique<T>(std::forward<T>(value));
	}
}
