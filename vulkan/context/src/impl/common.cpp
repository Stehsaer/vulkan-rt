#include "impl/common.hpp"

#include <algorithm>

namespace vulkan
{
	std::vector<std::string> operator-(
		const std::set<std::string>& a,
		const std::set<std::string>& b
	) noexcept
	{
		std::vector<std::string> result;
		std::ranges::set_difference(a, b, std::back_inserter(result));
		return result;
	}
}
