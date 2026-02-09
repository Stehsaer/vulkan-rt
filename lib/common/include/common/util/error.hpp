///
/// @file error.hpp
/// @brief Provides generic error type with stacktrace functionality
///

#pragma once

#include <algorithm>
#include <cstddef>
#include <expected>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <source_location>
#include <string>

// IWYU pragma: begin_exports
#include "common/formatter/vec.hpp"
#include "common/formatter/vulkan.hpp"
// IWYU pragma: end_exports

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
///   auto error = Error::from<T>(other_error);
///   ```
///
/// - Convert from another error type:
///   ```cpp
///   std::expected<T, E> result = ...;
///   std::expected<T, Error> new_result = result.transform_error(Error::from<E>());
///   ```
///   > Note: for custom type support, specialize `Error::from<T>::operator()`
///   > By default, types supported by `std::to_string`, and `vk::Result` are supported.
///
/// #### Propagating
///
/// - Forwarding an error with additional context:
///   ```cpp
///   error.forward("While doing something");
///   error.forward("While doing something", "With extra detail");
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
/// Use `Error::collect_vec()` to collect expected values into a vector, aggregating errors:
/// ```cpp
/// std::vector<std::expected<T, Error>> results = ...;
/// std::expected<std::vector<T>, Error> collected = std::move(results) | Error::collect_vec();
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

	std::string message;                 // Main message describing the error
	std::optional<std::string> detail;   // Optional detailed message providing additional context
	std::source_location location;       // Source location where the error was created or forwarded
	std::shared_ptr<const Error> cause;  // Pointer to the cause of the error, for chaining errors

  private:

	explicit Error(
		std::string message,
		std::optional<std::string> detail,
		std::source_location location,
		std::shared_ptr<const Error> cause
	) noexcept :
		message(std::move(message)),
		detail(std::move(detail)),
		location(location),
		cause(std::move(cause))
	{}

#pragma region Constructors

  public:

	///
	/// @brief Create an error with message
	///
	/// @param message Brief message describing the error
	/// @param location Source location where the error is created (default: current location)
	///
	explicit Error(
		std::string message,
		std::source_location location = std::source_location::current()
	) noexcept :
		message(std::move(message)),
		detail(std::nullopt),
		location(location),
		cause(nullptr)
	{}

	///
	/// @brief Create an error with message and extra detail
	///
	/// @param message Brief message describing the error
	/// @param detail Detailed message providing additional context
	/// @param location Source location where the error is created (default: current location)
	///
	explicit Error(
		std::string message,
		std::string detail,
		std::source_location location = std::source_location::current()
	) noexcept :
		message(std::move(message)),
		detail(std::move(detail)),
		location(location),
		cause(nullptr)
	{}

	///
	/// @brief Conversion functor to create Error from another error type
	/// @note
	/// To support custom types, implement a specialization of the `operator()` for the type T.
	///
	/// @tparam T The other error type
	///
	template <typename T>
	class FromFunctor
	{
		std::source_location location;

	  public:

		explicit FromFunctor(std::source_location location) noexcept :
			location(location)
		{}

		Error operator()(const T& error) const noexcept;
	};

	///
	/// @brief Get the conversion functor to create Error from another error type
	/// @details
	/// Example of usage:
	/// ```cpp
	/// std::expected<T, E> result = ...;
	/// std::expected<T, Error> new_result = result.transform_error(Error::from<E>());
	/// ```
	///
	/// @tparam T The other error type
	/// @param location Source location where the error is created (default: current location)
	/// @return Functor to convert T to Error
	///
	template <typename T>
	[[nodiscard]]
	static FromFunctor<T> from(std::source_location location = std::source_location::current()) noexcept
	{
		return FromFunctor<T>(location);
	}

	///
	/// @brief Convert another error type to Error
	///
	/// @tparam T The other error type
	/// @param error The other error instance
	/// @param location Source location where the error is created (default: current location)
	/// @return Converted Error instance
	///
	template <typename T>
	[[nodiscard]]
	static Error from(
		const T& error,
		std::source_location location = std::source_location::current()
	) noexcept
	{
		return FromFunctor<T>(location)(error);
	}

#pragma endregion

#pragma region Forwarding

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
		std::source_location location = std::source_location::current()
	) noexcept
	{
		return Error(
			std::move(message),
			std::nullopt,
			location,
			std::make_shared<const Error>(std::forward<decltype(self)>(self))
		);
	}

	///
	/// @brief Forward the error with additional context and detail
	///
	/// @param message Additional message describing the context
	/// @param detail Detailed message providing additional context
	/// @param location Source location where the error is forwarded (default: current location)
	/// @return New Error instance with forwarded context and detail
	///
	[[nodiscard]]
	Error forward(
		this auto&& self,
		std::string message,
		std::string detail,
		std::source_location location = std::source_location::current()
	) noexcept
	{
		return Error(
			std::move(message),
			std::move(detail),
			location,
			std::make_shared<const Error>(std::forward<decltype(self)>(self))
		);
	}

#pragma endregion

#pragma region Conversion

	operator std::unexpected<Error>() const noexcept { return std::unexpected(*this); }

	template <typename T>
	operator std::expected<T, Error>() const noexcept
	{
		return std::unexpected(*this);
	}

#pragma endregion

#pragma region Unwrapping

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
			{
				if (unwrap.detail)
					throw std::move(expected.error())
						.forward(unwrap.message, *unwrap.detail, unwrap.location);
				else
					throw std::move(expected.error()).forward(unwrap.message, unwrap.location);
			}

			return std::move(*expected);
		}

		friend void operator|(std::expected<void, Error> expected, const UnwrapFunctor& unwrap)
		{
			if (!expected)
			{
				if (unwrap.detail)
					throw std::move(expected.error())
						.forward(unwrap.message, *unwrap.detail, unwrap.location);
				else
					throw std::move(expected.error()).forward(unwrap.message, unwrap.location);
			}
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

#pragma endregion

#pragma region Iterating over error chain

  private:

	class Iterator
	{
		const Error* current;

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
			current(nullptr)
		{}

		explicit Iterator(const Error& error) noexcept :
			current(&error)
		{}

		/* Operators */

		reference operator*() const noexcept { return *current; }
		pointer operator->() const noexcept { return current; }

		Iterator& operator++() noexcept
		{
			current = current->cause.get();
			return *this;
		}

		Iterator operator++(int) noexcept
		{
			Iterator tmp = *this;
			++*this;
			return tmp;
		}

		bool operator==(const Iterator& other) const noexcept { return current == other.current; }
	};

	class ErrorChain
	{
		std::reference_wrapper<const Error> head;

	  public:

		ErrorChain(const ErrorChain&) = default;
		ErrorChain(ErrorChain&&) = default;
		ErrorChain& operator=(const ErrorChain&) = default;
		ErrorChain& operator=(ErrorChain&&) = default;

		explicit ErrorChain(const Error& error) noexcept :
			head(error)
		{}

		[[nodiscard]]
		Iterator begin() const noexcept
		{
			return Iterator(head.get());
		}

		[[nodiscard]]
		Iterator end() const noexcept
		{
			return {};
		}
	};

  public:

	///
	/// @brief Gets the error chain for iterating
	/// @details See `Error` documentation for usage examples
	///
	/// @return Error chain for iterating over the error and its causes
	///
	[[nodiscard]]
	ErrorChain chain() const noexcept
	{
		return ErrorChain(*this);
	}

#pragma endregion

#pragma region Collecting vectors

  private:

	class CollectVectorFunctor
	{
		std::source_location location;

	  public:

		explicit CollectVectorFunctor(std::source_location location) noexcept :
			location(location)
		{}

		template <typename T>
		friend std::expected<std::vector<T>, Error> operator|(
			std::vector<std::expected<T, Error>> range,
			const CollectVectorFunctor& collect
		) noexcept
		{
			std::vector<T> result;
			for (const auto [index, item] : range | std::views::enumerate)
			{
				if (!item)
					return item.error().forward(
						"Error in vector element",
						std::format("Error found in index {}", index),
						collect.location
					);
				result.emplace_back(std::move(*item));
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
	static CollectVectorFunctor collect_vec(
		std::source_location location = std::source_location::current()
	) noexcept
	{
		return CollectVectorFunctor(location);
	}

#pragma endregion
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
		std::string token(it, pos);

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
			return std::format_to(ctx.out(), "{}", err.message);

		case Kind::Detail:
			return std::format_to(ctx.out(), "{}", err.detail.value_or("<no detail>"));

		case Kind::Line:
			return std::format_to(ctx.out(), "{}", err.location.line());

		case Kind::Col:
			return std::format_to(ctx.out(), "{}", err.location.column());

		case Kind::File:
			return std::format_to(ctx.out(), "{}", err.location.file_name());

		case Kind::Func:
			return std::format_to(ctx.out(), "{}", err.location.function_name());

		case Kind::Location:
			return std::format_to(ctx.out(), "{}:{}", err.location.file_name(), err.location.line());

		case Kind::Default:
		default:
		{
			auto it = ctx.out();

			it = std::format_to(it, "({}:{}) {}", err.location.file_name(), err.location.line(), err.message);
			if (err.detail) it = std::format_to(it, ": {}", *err.detail);

			return it;
		}
		}

		throw std::format_error("Invalid format specifier");
	}
};

