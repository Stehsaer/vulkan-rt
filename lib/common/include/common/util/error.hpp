#pragma once

#include "common/json.hpp"
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <expected>
#include <format>
#include <iterator>
#include <libassert/assert.hpp>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <ranges>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

///
/// @brief Error class, supports error chaining and unwrapping
/// @details
///
/// #### Creating
///
/// - Simple error with message:
///   ```cpp
///   Error("Something went wrong")
///   ```
///
/// - Error with message and detail:
///   ```cpp
///   Error("Something went wrong", "Additional detail")
///   ```
///
/// - Create from another error type:
///   ```cpp
///   T other_error = ...;
///   auto error = Error::from(other_error);
///   ```
///
/// - Convert from another error type:
///   ```cpp
///   std::expected<T, E> result = ...;
///   std::expected<T, Error> new_result = result.transform_error(Error::from_fn());
///   ```
///   > Note: for custom type support, specialize `Error::from::operator<T>()`
///   > By default, types supported by `std::to_string`, and `vk::Result` are supported.
///
/// #### Visiting
///
/// Visit detailed information through `->` operator:
///
/// ```cpp
///	error->message;
/// error->detail;
/// ```
///
/// #### Propagating
///
/// - Forwarding an error with additional context:
///   ```cpp
///   error.forward("While doing something");
///   error.forward("While doing something", "With extra detail");
///   ```
///
/// - Use a forwarding functor for convenient forwarding in monads:
///   ```cpp
///   some_api().transform_error(Error::forward_func("While doing something"));
///   ```
///
/// - Error can be directly returned as `std::expected<T, Error>`:
///   ```cpp
///   std::expected<int, Error> func()
///   {
/// 	 return Error("An error occurred");
///   }
///   ```
///
/// #### Collecting
///
/// Use `Error::collect()` to collect expected values into a vector, aggregating errors:
/// ```cpp
/// std::vector<std::expected<T, Error>> results = ...;
/// std::expected<std::vector<T>, Error> collected = std::move(results) | Error::collect();
/// ```
///
/// #### Unwrapping
///
/// Use `Error::unwrap()` to unwrap expected values, throwing an Error if the value contains error:
/// ```cpp
/// std::expected<T, Error> result = ...;
/// T value = std::move(result) | Error::unwrap("Unwrapping failed", "Extra detail");
/// ```
///
/// #### Accessing Error Chain
///
/// `Error` provides convenient ways to access the entire chain of error.
///
/// - Iterate over the error chain:
///   ```cpp
///   for (const auto& entry : error.chain())
///   {
///       ... // process each entry
///   }
///   ```
///
/// - Alternatively, use C++20 ranges for more complex processing:
///   ```cpp
///   for (const auto& [idx, entry] : std::views::enumerate(error.chain()))
///   {
///       ... // process each entry with index
///   }
///   ```
///
/// - Call @p root to directly get the root cause:
///   ```cpp
///   std::println("{:msg}", error.root());
///   ```
///
/// #### Interpreting Error
///
/// Each error contains the following information:
/// - `message: std::string` - Main message describing the error
/// - `detail: std::optional<std::string>` - Optional detailed message providing additional context
/// - `location: std::source_location` - Source location where the error was created or forwarded
///
/// and
/// - `cause: std::shared_ptr<const Error>` - Pointer to the cause of the error, for chaining errors
/// Use them to interpret the error.
///
/// #### Formatting
///
/// This error class also supports formatting via `std::format`:
///
/// | Specifier | Description |
/// |-----------|-------------|
/// | `{}` | Brief representation including file, line and message |
/// | `{:msg}` / `{:message}` | Just the error message |
/// | `{:detail}` | Just the error detail, `"<no detail>"` if none |
/// | `{:loc}` / `{:location}` | File and line in `file:line` format |
/// | `{:file}` | Just the file name |
/// | `{:line}` | Just the line number |
/// | `{:col}` / `{:column}` | Just the column number |
/// | `{:func}` / `{:function}` | Just the function name |
///
/// Example:
/// ```cpp
/// std::format("Error occurred: {0:msg} at {0:location}", error);
/// std::format("Detail of the error: {0:detail}", error);
/// std::println("Print out error: {}", error);
/// ```
///
class Error
{
  public:

	///
	/// @brief Information block for an error
	///
	///
	class Record
	{
	  public:

		std::string message;                // Primary message
		std::optional<std::string> detail;  // Secondary / explanatory message
		Json diagnostics;                   // Binary diagnostics
		std::source_location location;      // Location of the error

	  private:

		friend class Error;

		std::shared_ptr<const Record> cause;  // Chain to next error information block

		Record(
			std::string message,
			std::optional<std::string> detail,
			Json diagnostics,
			std::source_location location,
			std::shared_ptr<const Record> cause
		);

	  public:

		Record(const Record&) = default;
		Record(Record&&) = default;
		Record& operator=(const Record&) = default;
		Record& operator=(Record&&) = default;
	};

  private:

	std::shared_ptr<const Record> storage;

	explicit Error(
		std::string message,
		std::optional<std::string> detail,
		Json diagnostics,
		std::shared_ptr<const Record> cause,
		std::source_location location
	) noexcept;

	explicit Error(std::shared_ptr<const Record> storage);

  public:

	[[nodiscard]]
	const Record* operator->() const noexcept
	{
		ASSUME(storage != nullptr);
		return storage.get();
	}

	///
	/// @brief Create an error with message
	///
	/// @param message Brief message describing the error
	/// @param location Source location where the error is created (default: current location)
	///
	explicit Error(
		std::string message,
		std::optional<std::string> detail = std::nullopt,
		Json diagnostics = {},
		std::source_location location = std::source_location::current()
	) noexcept;

	Error(const Error&) = default;
	Error(Error&&) = default;
	Error& operator=(const Error&) = default;
	Error& operator=(Error&&) = default;

	///
	/// @brief Convert another error type to Error
	///
	/// @tparam T The other error type
	/// @param error The other error instance
	/// @param location Source location where the error is created (default: current location)
	/// @return Converted Error instance
	///
	template <typename T>
		requires(!std::same_as<T, Error>)
	[[nodiscard]]
	static Error from(
		const T& error,
		Json diagnostics = {},
		std::source_location location = std::source_location::current()
	) noexcept;

	template <typename T, typename E>
		requires(!std::same_as<E, Error>)
	[[nodiscard]]
	static Error from(
		const std::expected<T, E>& error,
		Json diagnostics = {},
		std::source_location location = std::source_location::current()
	) noexcept
	{
		ASSUME(!error.has_value());
		return from(error.error(), std::move(diagnostics), location);
	}

	///
	/// @brief Forward the error with additional context
	///
	/// @param message Additional message describing the context
	/// @param location Source location where the error is forwarded (default: current location)
	/// @return New Error instance with forwarded context
	///
	[[nodiscard]]
	Error forward(
		this auto&& self,
		std::string message,
		std::optional<std::string> details = std::nullopt,
		Json diagnostics = {},
		std::source_location location = std::source_location::current()
	) noexcept
	{
		return Error(std::move(message), std::move(details), std::move(diagnostics), self.storage, location);
	}

	operator std::unexpected<Error>() const noexcept { return std::unexpected(*this); }

	template <typename T>
	operator std::expected<T, Error>() const noexcept
	{
		return std::unexpected(*this);
	}

  private:

	class UnwrapFunctor
	{
		std::string message;
		std::optional<std::string> detail;
		std::source_location location;

	  public:

		explicit UnwrapFunctor(std::string message, std::source_location location) noexcept :
			message(std::move(message)),
			location(location)
		{}

		explicit UnwrapFunctor(
			std::string message,
			std::string detail,
			std::source_location location
		) noexcept :
			message(std::move(message)),
			detail(std::move(detail)),
			location(location)
		{}

		template <typename T>
			requires(!std::same_as<T, void>)
		[[nodiscard]]
		friend T operator|(std::expected<T, Error> expected, const UnwrapFunctor& unwrap)
		{
			if (!expected)
				throw std::move(expected.error()).forward(unwrap.message, unwrap.detail, {}, unwrap.location);

			return std::move(*expected);
		}

		friend void operator|(std::expected<void, Error> expected, const UnwrapFunctor& unwrap)
		{
			if (!expected)
				throw std::move(expected.error()).forward(unwrap.message, unwrap.detail, {}, unwrap.location);
		}
	};

  public:

	///
	/// @brief Return the unwrapping functor to unwrap expected values
	///
	/// @param message Additional message describing the context
	/// @param location Source location where the unwrapping is done (default: current location)
	/// @return Unwrapping functor
	///
	[[nodiscard]]
	static UnwrapFunctor unwrap(
		std::string message = "",
		std::source_location location = std::source_location::current()
	) noexcept
	{
		return UnwrapFunctor(std::move(message), location);
	}

	///
	/// @brief Return the unwrapping functor to unwrap expected values with additional detail
	///
	/// @param message Additional message describing the context
	/// @param detail Detailed message providing additional context
	/// @param location Source location where the unwrapping is done (default: current location)
	/// @return Unwrapping functor
	///
	[[nodiscard]]
	static UnwrapFunctor unwrap(
		std::string message,
		std::string detail,
		std::source_location location = std::source_location::current()
	) noexcept
	{
		return UnwrapFunctor(std::move(message), std::move(detail), location);
	}

  private:

	friend class Iterator;

	class Iterator;
	class ErrorChain;

  public:

	///
	/// @brief Gets the error chain for iterating
	/// @details See `Error` documentation for usage examples
	///
	/// @return Error chain for iterating over the error and its causes
	///
	[[nodiscard]]
	ErrorChain chain() const noexcept;

	///
	/// @brief Gets the next error from the chain
	///
	/// @return Next error if exists, or empty if not exists
	///
	[[nodiscard]]
	std::optional<Error> next() const noexcept;

	///
	/// @brief Get the root cause
	///
	/// @return Last element of the error chain, which represents the root cause
	///
	[[nodiscard]]
	Error root() const noexcept;

  private:

	class CollectFunctor
	{
		std::source_location location;

		template <typename R>
		using RangeExpectedElementType =
			std::remove_cvref_t<decltype(*std::declval<std::ranges::range_value_t<R>>())>;

	  public:

		explicit CollectFunctor(std::source_location location) noexcept :
			location(location)
		{}

		template <std::ranges::viewable_range R>
		friend auto operator|(R range, const CollectFunctor& collect) noexcept
			-> std::expected<std::vector<RangeExpectedElementType<R>>, Error>
		{
			std::vector<RangeExpectedElementType<R>> result;

			size_t index = 0;
			for (auto&& item : range)
			{
				if (!item)
					return item.error().forward(
						"Error in range element",
						std::format("Error found in index {}", index),
						{},
						collect.location
					);
				result.emplace_back(std::move(*item));
				index++;
			}

			return result;
		}
	};

  public:

	///
	/// @brief Collects expected values into a vector, aggregating errors
	/// @details
	/// - Collecting will short-circuit on the first error encountered, returning that error. Additional
	/// context is attached, indicating the place in the vector where the error occurred.
	/// - See `Error` documentation for usage examples.
	///
	/// @param location Source location where the collection is done (default: current location)
	/// @return Collecting functor
	///
	[[nodiscard]]
	static CollectFunctor collect(std::source_location location = std::source_location::current()) noexcept
	{
		return CollectFunctor(location);
	}

	[[nodiscard]]
	Json to_json() const noexcept;
};

class Error::Iterator
{
	std::optional<Error> current;

  public:

	/* Iterator Traits */

	using iterator_concept = std::forward_iterator_tag;
	using value_type = Error;
	using difference_type = std::ptrdiff_t;
	using reference = const Error&;
	using pointer = const Error*;

	/* Ctors */

	Iterator(const Iterator&) noexcept = default;
	Iterator& operator=(const Iterator&) noexcept = default;
	Iterator(Iterator&&) noexcept = default;
	Iterator& operator=(Iterator&&) noexcept = default;

	Iterator() noexcept :
		current(std::nullopt)
	{}

	explicit Iterator(Error err) noexcept :
		current(std::move(err))
	{}

	/* Operators */

	reference operator*() const noexcept;
	pointer operator->() const noexcept;
	Iterator& operator++() noexcept;
	Iterator operator++(int) noexcept;
	bool operator==(const Iterator& other) const noexcept;
};

class Error::ErrorChain
{
	Error head;

  public:

	ErrorChain(const ErrorChain&) = default;
	ErrorChain(ErrorChain&&) = default;
	ErrorChain& operator=(const ErrorChain&) = default;
	ErrorChain& operator=(ErrorChain&&) = default;

	explicit ErrorChain(Error error) noexcept :
		head(std::move(error))
	{}

	[[nodiscard]]
	Iterator begin() const noexcept
	{
		return Iterator(head);
	}

	[[nodiscard]]
	Iterator end() const noexcept
	{
		return {};
	}
};

template <>
class std::formatter<Error, char>
{
	enum class Kind
	{
		Default,
		Message,
		Detail,
		Location,
		Line,
		Col,
		File,
		Func
	} kind = Kind::Default;

  public:

	template <typename ParseContext>
	constexpr ParseContext::iterator parse(ParseContext& ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();
		if (it == end || *it == '}') return it;

		auto pos = std::find(it, end, '}');
		const std::string token(it, pos);

		if (token == "msg" || token == "message")
			kind = Kind::Message;
		else if (token == "detail")
			kind = Kind::Detail;
		else if (token == "line")
			kind = Kind::Line;
		else if (token == "file")
			kind = Kind::File;
		else if (token == "col" || token == "column")
			kind = Kind::Col;
		else if (token == "func" || token == "function")
			kind = Kind::Func;
		else if (token == "loc" || token == "location")
			kind = Kind::Location;
		else
			throw std::format_error("Invalid format specifier");

		return pos;
	}

	template <typename FormatContext>
	FormatContext::iterator format(const Error& err, FormatContext& ctx) const
	{
		switch (kind)
		{
		case Kind::Message:
			return std::format_to(ctx.out(), "{}", err->message);

		case Kind::Detail:
			return std::format_to(ctx.out(), "{}", err->detail.value_or("<no detail>"));

		case Kind::Line:
			return std::format_to(ctx.out(), "{}", err->location.line());

		case Kind::Col:
			return std::format_to(ctx.out(), "{}", err->location.column());

		case Kind::File:
			return std::format_to(ctx.out(), "{}", err->location.file_name());

		case Kind::Func:
			return std::format_to(ctx.out(), "{}", err->location.function_name());

		case Kind::Location:
			return std::format_to(ctx.out(), "{}:{}", err->location.file_name(), err->location.line());

		case Kind::Default:
		default:
		{
			auto it = ctx.out();

			it = std::format_to(
				it,
				"{} ({}:{})",
				err->message,
				err->location.file_name(),
				err->location.line()
			);
			if (err->detail) it = std::format_to(it, ": {}", *err->detail);

			return it;
		}
		}
	}
};
