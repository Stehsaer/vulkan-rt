#include "common/util/error.hpp"

#include <exception>
#include <filesystem>
#include <format>
#include <iterator>
#include <libassert/assert.hpp>
#include <system_error>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>  // Silent clang-tidy include cleaners

template <>
Error Error::FromFunctor::operator()(const vk::Result& e) const noexcept
{
	return Error("Vulkan operation failed", vk::to_string(e), location);
}

template <>
Error Error::FromFunctor::operator()(const std::error_code& e) const noexcept
{
	return Error(std::format("System error: {}", e.message()), std::format("Code: {}", e.value()), location);
}

template <>
Error Error::FromFunctor::operator()(const std::filesystem::filesystem_error& e) const noexcept
{
	return Error(
		std::format("Filesystem error: {}", e.what()),
		std::format(
			"Path1: '{}', Path2: '{}', Code: {} ({})",
			e.path1().string(),
			e.path2().string(),
			e.code().message(),
			e.code().value()
		),
		location
	);
}

template <>
Error Error::FromFunctor::operator()(const std::exception& e) const noexcept
{
	return Error(std::format("Error (std::exception): {}", e.what()), location);
}

const Error& Error::root() const noexcept
{
	const auto error_chain = chain();
	const auto dist = std::distance(error_chain.begin(), error_chain.end());
	ASSUME(dist >= 1);

	return *std::next(error_chain.begin(), dist - 1);
}
