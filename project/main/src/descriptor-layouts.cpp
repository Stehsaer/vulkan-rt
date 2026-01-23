#include "descriptor-layouts.hpp"

namespace
{
	constexpr auto bindings = std::to_array<vk::DescriptorSetLayoutBinding>({
		{.binding = 0,
		 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
		 .descriptorCount = 1,
		 .stageFlags = vk::ShaderStageFlagBits::eFragment}
	});
}

std::expected<DescriptorLayouts, Error> DescriptorLayouts::create(const vk::raii::Device& device) noexcept
{
	const auto create_info = vk::DescriptorSetLayoutCreateInfo{}.setBindings(bindings);

	auto descriptor_layout_expected = device.createDescriptorSetLayout(create_info);
	if (!descriptor_layout_expected)
		return Error(descriptor_layout_expected.error(), "Create descriptor set layout failed");

	return DescriptorLayouts{.main_layout = std::move(*descriptor_layout_expected)};
}
