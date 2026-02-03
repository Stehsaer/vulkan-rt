#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan::context
{
	static constexpr uint32_t api_version = vk::ApiVersion14;

	std::vector<std::string> operator-(
		const std::set<std::string>& a,
		const std::set<std::string>& b
	) noexcept;
}