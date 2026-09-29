
#include "common/container/error.hpp"
#include "common/container/debug-trace.hpp"
#include "common/formatter.hpp"

#include <exception>
#include <format>
#include <libassert/assert.hpp>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <system_error>
#include <utility>
#include <vulkan/vulkan.hpp>

Error::Error(
	std::string message,
	std::optional<std::string> detail,
	std::optional<debug::Trace> trace,
	std::source_location location
) noexcept :
	Error(std::move(message), std::move(detail), std::move(trace), nullptr, location)
{}

Error::Error(
	std::string message,
	std::optional<std::string> detail,
	std::optional<debug::Trace> trace,
	std::shared_ptr<const Record> cause,
	std::source_location location
) noexcept :
	storage(
		std::make_shared<Record>(
			Record(std::move(message), std::move(detail), std::move(trace), location, std::move(cause))
		)
	)
{}

Error::Error(std::shared_ptr<const Record> storage) :
	storage(std::move(storage))
{
	DEBUG_ASSERT(this->storage != nullptr);
}

Error::Record::Record(
	std::string message,
	std::optional<std::string> detail,
	std::optional<debug::Trace> trace,
	std::source_location location,
	std::shared_ptr<const Record> cause
) noexcept :
	message(std::move(message)),
	detail(std::move(detail)),
	trace(std::move(trace)),
	location(location),
	cause(std::move(cause))
{}

Error::ErrorChain Error::chain() const noexcept
{
	return ErrorChain(*this);
}

std::optional<Error> Error::next() const noexcept
{
	if (storage->cause)
		return Error(storage->cause);
	else
		return std::nullopt;
}

Error Error::root() const noexcept
{
	Error err = *this;
	while (err.next()) err = err.next().value();
	return err;
}

Error::Iterator::reference Error::Iterator::operator*() const noexcept
{
	ASSUME(current.has_value());
	return *current;
}

Error::Iterator::pointer Error::Iterator::operator->() const noexcept
{
	ASSUME(current.has_value());
	return &(*current);
}

Error::Iterator& Error::Iterator::operator++() noexcept
{
	ASSUME(current.has_value());
	current = current->next();
	return *this;
}

Error::Iterator Error::Iterator::operator++(int) noexcept
{
	Iterator tmp = *this;
	++*this;
	return tmp;
}

bool Error::Iterator::operator==(const Iterator& other) const noexcept
{
	if (!current) return !other.current.has_value();
	if (!other.current) return false;
	return current->storage.get() == other.current->storage.get();
}

template <>
Error Error::from(
	const vk::Result& e,
	std::optional<debug::Trace> trace,
	std::source_location location
) noexcept
{
	return Error(
		"Vulkan-related error occurred",
		std::format("Error code: {}", e),
		std::move(trace),
		location
	);
}

template <>
Error Error::from(
	const std::error_code& e,
	std::optional<debug::Trace> trace,
	std::source_location location
) noexcept
{
	return Error(
		"System error occurred",
		std::format("Code: {} ({})", e.message(), e.value()),
		std::move(trace),
		location
	);
}

template <>
Error Error::from(
	const std::exception& e,
	std::optional<debug::Trace> trace,
	std::source_location location
) noexcept
{
	return Error("Unknown error", std::format("Message: {}", e.what()), std::move(trace), location);
}
