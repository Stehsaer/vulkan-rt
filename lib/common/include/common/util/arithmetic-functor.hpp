#pragma once

#include <concepts>
#include <functional>

namespace util
{
	template <typename T>
	concept Number = std::integral<T> || std::floating_point<T>;

	template <typename Pred, Number T>
	class CompareToValue
	{
		T value;

	  public:

		CompareToValue(const CompareToValue&) = default;
		CompareToValue(CompareToValue&&) = default;
		CompareToValue& operator=(const CompareToValue&) = default;
		CompareToValue& operator=(CompareToValue&&) = default;

		explicit CompareToValue(T value) :
			value(value)
		{}

		template <Number U>
		[[nodiscard]]
		bool operator()(U other) const noexcept
		{
			using CommonType = std::common_type_t<T, U>;
			return Pred{}(static_cast<CommonType>(other), static_cast<CommonType>(value));
		}
	};

	template <Number T>
	using LessThanValue = CompareToValue<std::ranges::less, T>;

	template <Number T>
	using LessEqualToValue = CompareToValue<std::ranges::less_equal, T>;

	template <Number T>
	using GreaterEqualToValue = CompareToValue<std::ranges::greater_equal, T>;

	template <Number T>
	using GreaterThanValue = CompareToValue<std::ranges::greater, T>;

	template <Number T>
	using EqualToValue = CompareToValue<std::ranges::equal_to, T>;

	template <Number T>
	using NotEqualToValue = CompareToValue<std::ranges::not_equal_to, T>;
}
