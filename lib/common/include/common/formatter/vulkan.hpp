///
/// @file vulkan.hpp
/// @brief Provides std::formatter specializations for Vulkan types, based on `vk::to_string`
///

#pragma once

#include <format>
#include <ranges>
#include <vulkan/vulkan_to_string.hpp>

namespace vk_formatter
{
	template <typename T>
	concept FormattableType = requires(const T& value, std::string& out_str) {
		{ vk::to_string(value) } -> std::convertible_to<std::string>;
	};
}

template <vk_formatter::FormattableType T>
struct std::formatter<T, char> : std::formatter<std::string, char>
{
	auto format(const T& value, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "{}", vk::to_string(value));
	}
};

template <std::ranges::range Range>
	requires vk_formatter::FormattableType<std::ranges::range_value_t<Range>>
struct std::formatter<Range, char>
{
	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

	auto format(const Range& range, std::format_context& ctx) const
	{
		return std::format_to(
			ctx.out(),
			"[{}]",
			range
				| std::views::transform([](const auto& value) { return vk::to_string(value); })
				| std::views::join_with(std::string(", "))
				| std::ranges::to<std::string>()
		);
	}
};