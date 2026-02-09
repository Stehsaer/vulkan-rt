#include "common/util/error.hpp"

#include <vulkan/vulkan_to_string.hpp>

template <>
Error Error::FromFunctor<vk::Result>::operator()(const vk::Result& result) const noexcept
{
	return Error("Vulkan operation failed", vk::to_string(result), location);
}