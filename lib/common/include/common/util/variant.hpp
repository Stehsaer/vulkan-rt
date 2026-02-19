#pragma once

#include <functional>
#include <optional>
#include <variant>

namespace util
{
	template <typename GetType, typename... VariantTypes>
	[[nodiscard]]
	std::optional<std::reference_wrapper<GetType>> get_variant(std::variant<VariantTypes...>& var) noexcept
	{
		if (std::holds_alternative<GetType>(var))
			return std::ref(std::get<GetType>(var));
		else
			return std::nullopt;
	}

	template <typename GetType, typename... VariantTypes>
	[[nodiscard]]
	std::optional<std::reference_wrapper<const GetType>> get_variant(
		const std::variant<VariantTypes...>& var
	) noexcept
	{
		if (std::holds_alternative<GetType>(var))
			return std::cref(std::get<GetType>(var));
		else
			return std::nullopt;
	}
}
