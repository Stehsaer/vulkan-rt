#include <doctest.h>
#include <ranges>
#include <vulkan/vulkan.hpp>

#include "vulkan/util/pool-size.hpp"

TEST_CASE("Trivial")
{
	constexpr auto binding1 = vk::DescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = vk::DescriptorType::eStorageBuffer,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eCompute
	};

	constexpr auto binding2 = vk::DescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = vk::DescriptorType::eStorageBuffer,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eCompute
	};

	constexpr auto binding3 = vk::DescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eCompute
	};

	constexpr auto bindings = std::to_array({
		binding1,
		binding2,
		binding3,
	});

	const auto pool_sizes =
		vulkan::calc_pool_sizes(bindings, 2)
		| std::views::transform([](const auto& pool_size) {
			  return std::make_pair(pool_size.type, pool_size.descriptorCount);
		  })
		| std::ranges::to<std::map>();

	CHECK(pool_sizes.contains(vk::DescriptorType::eStorageBuffer));
	CHECK(pool_sizes.contains(vk::DescriptorType::eCombinedImageSampler));
	CHECK_EQ(pool_sizes.at(vk::DescriptorType::eStorageBuffer), 2 * 2);
	CHECK_EQ(pool_sizes.at(vk::DescriptorType::eCombinedImageSampler), 1 * 2);
}
