
#include "common/util/error.hpp"
#include "common/formatter.hpp"
#include "common/json.hpp"

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
#include <vulkan/vulkan_to_string.hpp>  // Silent clang-tidy include cleaners

Error::Error(
	std::string message,
	std::optional<std::string> detail,
	Json diagnostics,
	std::source_location location
) noexcept :
	Error(std::move(message), std::move(detail), std::move(diagnostics), nullptr, location)
{}

Error::Error(
	std::string message,
	std::optional<std::string> detail,
	Json diagnostics,
	std::shared_ptr<const Record> cause,
	std::source_location location
) noexcept :
	storage(
		std::make_shared<Record>(
			Record(std::move(message), std::move(detail), std::move(diagnostics), location, std::move(cause))
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
	Json diagnostics,
	std::source_location location,
	std::shared_ptr<const Record> cause
) :
	message(std::move(message)),
	detail(std::move(detail)),
	diagnostics(std::move(diagnostics)),
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

Json Error::to_json() const noexcept
{
	return {
		{"message",     storage->message             },
		{"detail",      storage->detail              },
		{"diagnostics", storage->diagnostics         },
		{"file",        storage->location.file_name()},
		{"line",        storage->location.line()     }
	};
}

template <>
Error Error::from(const vk::Result& e, Json diagnostics, std::source_location location) noexcept
{
	return Error(
		"Vulkan-related error occurred",
		std::format("Error code: {}", e),
		std::move(diagnostics),
		location
	);
}

template <>
Error Error::from(const std::error_code& e, Json diagnostics, std::source_location location) noexcept
{
	return Error(
		"System error occurred",
		std::format("Code: {} ({})", e.message(), e.value()),
		std::move(diagnostics),
		location
	);
}

template <>
Error Error::from(const std::exception& e, Json diagnostics, std::source_location location) noexcept
{
	return Error("Unknown error", std::format("Message: {}", e.what()), std::move(diagnostics), location);
}
