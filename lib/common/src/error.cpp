#include "common/utility/error.hpp"

#include <SDL3/SDL.h>
#include <format>
#include <vulkan/vulkan_to_string.hpp>

Error::Error(vk::Result result, const std::string& message, std::source_location location) noexcept
{
	trace.emplace_back(
		TraceEntry{
			.message = std::format("{} (vk::Result={})", message, vk::to_string(result)),
			.location = location
		}
	);
}

Error::Error(const std::string& message, std::source_location location) noexcept
{
	trace.emplace_back(TraceEntry{.message = message, .location = location});
}

Error::Error(Error::FromSDL_type, const std::string& message, std::source_location location) noexcept
{
	trace.emplace_back(
		TraceEntry{.message = std::format("{} (SDL_Error={})", message, SDL_GetError()), .location = location}
	);
}

Error Error::forward(const std::string& message, std::source_location location) && noexcept
{
	auto error = Error(std::move(*this));
	error.trace.emplace_back(TraceEntry{.message = message, .location = location});
	return error;
}

Error Error::forward(const std::string& message, std::source_location location) const& noexcept
{
	auto error = Error(*this);
	error.trace.emplace_back(TraceEntry{.message = message, .location = location});
	return error;
}

void Error::throw_self(const std::string& message, std::source_location location) const
{
	throw this->forward(message, location);
}
