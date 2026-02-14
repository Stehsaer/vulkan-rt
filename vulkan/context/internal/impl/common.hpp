#pragma once

#include <set>
#include <string>
#include <vector>

namespace vulkan
{
	std::vector<std::string> operator-(
		const std::set<std::string>& a,
		const std::set<std::string>& b
	) noexcept;
}