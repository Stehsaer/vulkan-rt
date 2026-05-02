#include "vulkan/util/pool-size.hpp"
#include "common/util/unpack.hpp"

#include <map>
#include <ranges>

namespace vulkan
{
	std::vector<vk::DescriptorPoolSize> calc_pool_sizes(
		std::span<const vk::DescriptorSetLayoutBinding> bindings,
		uint32_t set_count
	) noexcept
	{
		std::map<vk::DescriptorType, uint32_t> descriptor_counts;
		for (const auto& binding : bindings)
		{
			const auto find = descriptor_counts.find(binding.descriptorType);
			if (find == descriptor_counts.end())
				descriptor_counts.insert({binding.descriptorType, binding.descriptorCount * set_count});
			else
				find->second += binding.descriptorCount * set_count;
		}

		return descriptor_counts
			| std::views::transform([](auto type, auto count) {
				   return vk::DescriptorPoolSize{.type = type, .descriptorCount = count};
			   } | util::tuple_args)
			| std::ranges::to<std::vector>();
	}
}
