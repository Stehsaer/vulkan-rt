#include "impl/common.hpp"

#include <algorithm>
#include <iterator>

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

	std::set<std::string> operator+(const std::set<std::string>& a, const std::set<std::string>& b) noexcept
	{
		std::set dup = a;
		dup.insert_range(b);

		return dup;
	}
}
