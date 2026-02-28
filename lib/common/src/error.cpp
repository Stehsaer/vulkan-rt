#include "common/util/error.hpp"

#include <exception>
#include <filesystem>
#include <system_error>
#include <vulkan/vulkan_to_string.hpp>

template <>
Error Error::FromFunctor<vk::Result>::operator()(const vk::Result& result) const noexcept
{
	return Error("Vulkan operation failed", vk::to_string(result), location);
}

template <>
Error Error::FromFunctor<std::error_code>::operator()(const std::error_code& ec) const noexcept
{
	return Error(
		std::format("System error: {}", ec.message()),
		std::format("Code: {}", ec.value()),
		location
	);
}

template <>
Error Error::FromFunctor<std::filesystem::filesystem_error>::operator()(
	const std::filesystem::filesystem_error& fe
) const noexcept
{
	return Error(
		std::format("Filesystem error: {}", fe.what()),
		std::format(
			"Path1: '{}', Path2: '{}', Code: {} ({})",
			fe.path1().string(),
			fe.path2().string(),
			fe.code().message(),
			fe.code().value()
		),
		location
	);
}

template <>
Error Error::FromFunctor<std::exception>::operator()(const std::exception& e) const noexcept
{
	return Error(std::format("Error (std::exception): {}", e.what()), location);
}
