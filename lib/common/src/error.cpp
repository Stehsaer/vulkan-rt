#include "common/util/error.hpp"
#include "common/formatter.hpp"
#include "common/json.hpp"

#include <exception>
#include <format>
#include <libassert/assert.hpp>
#include <source_location>
#include <system_error>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>  // Silent clang-tidy include cleaners

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

Error Error::root() const noexcept
{
	Error err = *this;
	while (err.next()) err = err.next().value();
	return err;
}
