///
/// @file error.hpp
/// @brief Provides generic error type with trace functionality
///

#pragma once

#include <algorithm>
#include <expected>
#include <format>
#include <ranges>
#include <source_location>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

// IWYU pragma: begin_exports
#include "common/formatter/vec.hpp"
#include "common/formatter/vulkan.hpp"
// IWYU pragma: end_exports

///
/// @brief Struct representing an error with a trace of its propagation
///
/// @details
///
/// ### Creation
/// - From a `vk::Result` code: `Error(vk::Result, "message")`
/// - From an SDL error: `Error(Error::from_sdl, "message")`
/// - From a message directly: `Error("message")`
///
/// ---
///
/// ### Error Handling
///
/// #### Cascading/Forwarding
///
/// Forwarding the error adds a new trace entry:
/// ```cpp
/// if (!some_expected)
/// 	return some_expected.error().forward("Additional message");
/// ```
///
/// The message is optional; if omitted, only the location is recorded:
/// ```cpp
/// if (!some_expected)
/// 	return some_expected.error().forward();
/// ```
///
/// #### Throwing
///
/// If necessary and exceptions are enabled, the error can be thrown with additional context:
/// ```cpp
/// if (!some_expected)
/// 	some_expected.error().throw_self("Additional message");
/// ```
/// Catch the error directly:
/// ```cpp
/// try {...}
/// catch (const Error& error)
/// {
///    // Handle the error
/// }
/// ```
///
/// #### Collecting
///
/// Multiple expected values stored in `std::vector` can be collected into a vector, short circuiting on the
/// first element with error:
/// ```cpp
/// std::vector<std::expected<T, Error>> expected_list = ...;
/// std::expected<std::vector<T>, Error> list_expected
///     = std::move(expected_list) | Error::collect_vec("Collecting objects failed");
/// // Error handling...
/// ```
///
/// ---
///
/// ### Accessing and Using Trace Entries
///
/// Each trace entry contains:
/// - A message
/// - A source location pointing to where the error was created or
/// forwarded.
///
/// Acquire the original error location with `error.origin()`, which returns the first and deepest trace
/// entry. The full trace can be accessed via the `trace` member, where smaller indices are deeper in the call
/// stack.
///
/// After acquiring a trace entry, its fields can be formatted using `std::format`.
///
/// | Specifier | Description |
/// | --- | --- |
/// | `{}` | Full message (function, file, line, message) |
/// | `{:msg}` | Message |
/// | `{:line}` | Line number |
/// | `{:col}` | Column number |
/// | `{:file}` | File name |
/// | `{:func}` | Function name |
///
struct Error
{
	///
	/// @brief Trace entry struct, containing a message and source location
	/// @note For formatting, see documentation for `Error`
	///
	struct TraceEntry
	{
		std::string message;
		std::source_location location;
	};

	inline static const struct FromSDL_type
	{
	} from_sdl;

	///
	/// @brief Trace entries
	/// @details Each entry represents a point in the call stack where the error was propagated or
	/// handled. `.front()` is the deepest call, `.back()` is the most recent.
	///
	std::vector<TraceEntry> trace;

	///
	/// @brief Create an Error from a Vulkan result code
	///
	/// @param result Vulkan result code
	/// @param message Optional message to include in the trace entry
	/// @param location Source location of the error
	/// @return Error instance
	///
	Error(
		vk::Result result,
		const std::string& message = "",
		std::source_location location = std::source_location::current()
	) noexcept;

	///
	/// @brief Create an Error directly from a message
	///
	/// @param message Optional message to include in the trace entry
	/// @param location Source location of the error
	/// @return
	///
	Error(
		const std::string& message = "",
		std::source_location location = std::source_location::current()
	) noexcept;

	///
	/// @brief Create an Error from an SDL error
	///
	/// @param message Optional message to include in the trace entry
	/// @param location Source location of the error
	///
	Error(
		FromSDL_type,
		const std::string& message = "",
		std::source_location location = std::source_location::current()
	) noexcept;

	Error(const Error&) = default;
	Error(Error&&) = default;
	Error& operator=(const Error&) = default;
	Error& operator=(Error&&) = default;

	template <typename V>
	operator std::expected<V, Error>(this auto&& self) noexcept
	{
		return std::unexpected<Error>(std::forward<decltype(self)>(self));
	}

	///
	/// @brief Forward the error by adding a new trace entry
	///
	/// @param message Message to include in the trace entry
	/// @param location Source location of the forwarding
	/// @return New Error instance with added trace entry
	///
	[[nodiscard]]
	Error forward(
		const std::string& message = "",
		std::source_location location = std::source_location::current()
	) && noexcept;

	///
	/// @brief Forward the error by adding a new trace entry
	///
	/// @param message Message to include in the trace entry
	/// @param location Source location of the forwarding
	/// @return New Error instance with added trace entry
	///
	[[nodiscard]]
	Error forward(
		const std::string& message = "",
		std::source_location location = std::source_location::current()
	) const& noexcept;

	///
	/// @brief Throw the error as an exception
	///
	/// @param message Additional message to include in the trace entry
	/// @param location Source location of the throwing
	///
	[[noreturn]]
	void throw_self(
		const std::string& message = "",
		std::source_location location = std::source_location::current()
	) const;

	///
	/// @brief Get the original error trace entry
	///
	/// @return Original trace entry
	///
	const TraceEntry& origin() const noexcept { return trace.front(); }

	struct CollectFunctor
	{
		const std::source_location invoke_loc;
		const std::string message;
	};

	///
	/// @brief Return a functor to collect expected values into a vector, propagating errors
	///
	/// @param message Optional message to include in the trace entry
	/// @param location Source location of the invocation
	/// @return CollectFunctor instance
	///
	static CollectFunctor collect_vec(
		std::string message = "",
		std::source_location location = std::source_location::current()
	) noexcept
	{
		return CollectFunctor{.invoke_loc = location, .message = std::move(message)};
	}
};

template <std::move_constructible T>
std::expected<std::vector<T>, Error> operator|(
	std::vector<std::expected<T, Error>> expected,
	const Error::CollectFunctor& func
) noexcept
{
	std::vector<T> result;
	result.reserve(expected.size());

	for (const auto [idx, item] : std::views::enumerate(expected))
	{
		if (!item)
			return item.error().forward(
				func.message.empty()
					? std::format("Found error at index {}", idx)
					: std::format("{} (at index {})", func.message, idx),
				func.invoke_loc
			);
		result.emplace_back(std::move(*item));
	}

	return result;
}

template <>
struct std::formatter<Error::TraceEntry, char>
{
	enum class Kind
	{
		Full,
		Msg,
		Line,
		Col,
		File,
		Func
	};

	Kind kind = Kind::Full;

	template <typename ParseContext>
	constexpr ParseContext::iterator parse(ParseContext& ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();
		if (it == end || *it == '}') return it;

		auto pos = std::find(it, end, '}');
		std::string token(it, pos);

		if (token == "msg")
			kind = Kind::Msg;
		else if (token == "line")
			kind = Kind::Line;
		else if (token == "col")
			kind = Kind::Col;
		else if (token == "file")
			kind = Kind::File;
		else if (token == "func")
			kind = Kind::Func;
		else
			throw format_error("invalid format specifier for TraceEntry");

		return pos;
	}

	template <typename FormatContext>
	typename FormatContext::iterator format(const Error::TraceEntry& entry, FormatContext& ctx) const
	{
		switch (kind)
		{
		case Kind::Msg:
			return std::format_to(ctx.out(), "{}", entry.message);
		case Kind::Line:
			return std::format_to(ctx.out(), "{}", entry.location.line());
		case Kind::Col:
			return std::format_to(ctx.out(), "{}", entry.location.column());
		case Kind::File:
			return std::format_to(ctx.out(), "{}", entry.location.file_name());
		case Kind::Func:
			return std::format_to(ctx.out(), "{}", entry.location.function_name());
		case Kind::Full:
			return std::format_to(
				ctx.out(),
				"{} ({}:{}): {}",
				entry.location.function_name(),
				entry.location.file_name(),
				entry.location.line(),
				entry.message
			);
		}

		throw format_error("Invalid format specifier for TraceEntry");
	}
};
